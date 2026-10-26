#include "pch.h"
#include "net.h"
#include "config.h"
#include "log.h"
#include "eos.h"
#include <bcrypt.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace pdnet::net
{
    namespace
    {
        constexpr uint8_t kSnapshotMagic[4] = { 'P', 'D', 'N', '1' };
        constexpr uint8_t kMsgMagic[4] = { 'P', 'D', 'N', 'C' };
        constexpr size_t kHeader = 4 + 1 + 4;

        std::function<void(const Message&)> g_onUnsolicited;
        SOCKET g_socket = INVALID_SOCKET;

        struct Pending
        {
            winrt::handle event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
            bool done = false;
            Message reply;
        };
        std::mutex g_pendingLock;
        std::map<uint32_t, std::shared_ptr<Pending>> g_pending;

        uint64_t Random64()
        {
            uint64_t v = 0;
            BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&v), sizeof(v), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            return v;
        }

        void Put16(std::vector<uint8_t>& b, uint16_t v)
        {
            b.push_back(static_cast<uint8_t>(v >> 8));
            b.push_back(static_cast<uint8_t>(v & 0xff));
        }

        uint16_t Get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }

        std::array<uint8_t, 16> Mapped(const in_addr& a)
        {
            std::array<uint8_t, 16> ip{};
            ip[10] = ip[11] = 0xff;
            memcpy(&ip[12], &a, 4);
            return ip;
        }

        // Source address Windows would use for internet traffic (connecting a UDP socket sends nothing).
        bool PrimaryIpv4(in_addr& out)
        {
            SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET) return false;
            sockaddr_in dst{};
            dst.sin_family = AF_INET;
            dst.sin_port = htons(53);
            inet_pton(AF_INET, "8.8.8.8", &dst.sin_addr);
            sockaddr_in src{};
            int len = sizeof(src);
            bool ok = connect(s, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) == 0 &&
                      getsockname(s, reinterpret_cast<sockaddr*>(&src), &len) == 0;
            closesocket(s);
            if (ok) out = src.sin_addr;
            return ok;
        }

        Snapshot MakeLocal()
        {
            WSADATA wsa;
            WSAStartup(MAKEWORD(2, 2), &wsa);
            const Config& config = GetConfig();

            Snapshot s;
            s.deviceId = Random64();
            s.gamePort = config.gamePort;
            s.controlPort = config.controlPort;

            bool haveIp = false;
            if (config.advertiseIp != L"auto")
            {
                in_addr v4;
                if (InetPtonW(AF_INET, config.advertiseIp.c_str(), &v4) == 1)
                {
                    s.ip = Mapped(v4);
                    haveIp = true;
                }
                else if (InetPtonW(AF_INET6, config.advertiseIp.c_str(), s.ip.data()) == 1)
                {
                    haveIp = true;
                }
                else
                {
                    Log(L"net: advertise_ip '%s' is not an IP address, using auto", config.advertiseIp.c_str());
                }
            }
            if (!haveIp)
            {
                in_addr v4;
                if (PrimaryIpv4(v4))
                {
                    s.ip = Mapped(v4);
                }
                else
                {
                    Log(L"net: could not determine this PC's address (error %d)", WSAGetLastError());
                    s.ip = Mapped(in_addr{});
                }
            }
            if (config.p2p)
            {
                // Peers reach us through EOS P2P by product user id, so wait for the EOS sign-in (normally long done).
                for (int i = 0; i < 150 && !eos::SignedIn(); ++i) Sleep(100);
                std::string user = winrt::to_string(eos::LocalUserId());
                if (user.size() == 32) s.eosUser = user;
                else Log(L"net: network_mode=eos but EOS is not signed in; other players will use direct UDP to reach this PC");
            }
            Log(L"net: local device %s", s.Describe().c_str());
            return s;
        }

        void ReceiveLoop()
        {
            std::vector<uint8_t> buffer(64 * 1024);
            for (;;)
            {
                sockaddr_in6 from{};
                int fromLen = sizeof(from);
                int n = recvfrom(g_socket, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                                 reinterpret_cast<sockaddr*>(&from), &fromLen);
                if (n == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSAECONNRESET || err == WSAEMSGSIZE) continue; // ICMP port unreachable / oversized datagram
                    Log(L"net: control socket receive failed (%d), stopping", err);
                    return;
                }
                Message m;
                if (!Decode(buffer.data(), static_cast<size_t>(n), m)) continue;
                m.from = from;

                if (m.type == MsgType::Ack || m.type == MsgType::Pong)
                {
                    std::shared_ptr<Pending> pending;
                    {
                        std::lock_guard guard(g_pendingLock);
                        if (auto it = g_pending.find(m.nonce); it != g_pending.end()) pending = it->second;
                    }
                    if (pending)
                    {
                        {
                            std::lock_guard guard(g_pendingLock);
                            pending->reply = std::move(m);
                            pending->done = true;
                        }
                        SetEvent(pending->event.get());
                        continue;
                    }
                }

                try
                {
                    if (g_onUnsolicited) g_onUnsolicited(m);
                }
                catch (...)
                {
                    Log(L"net: handler threw 0x%08X", winrt::to_hresult());
                }
            }
        }
    }

    std::vector<uint8_t> Encode(MsgType type, uint32_t nonce, const std::vector<uint8_t>& body)
    {
        std::vector<uint8_t> packet(kMsgMagic, kMsgMagic + 4);
        packet.push_back(static_cast<uint8_t>(type));
        auto n = reinterpret_cast<const uint8_t*>(&nonce);
        packet.insert(packet.end(), n, n + 4);
        packet.insert(packet.end(), body.begin(), body.end());
        return packet;
    }

    bool Decode(const uint8_t* data, size_t size, Message& out)
    {
        if (size < kHeader || memcmp(data, kMsgMagic, 4) != 0) return false;
        out.type = static_cast<MsgType>(data[4]);
        memcpy(&out.nonce, data + 5, 4);
        out.body.assign(data + kHeader, data + size);
        return true;
    }

    std::array<uint8_t, 16> InterfaceIp()
    {
        static std::array<uint8_t, 16> ip = []
        {
            Local(); // WSAStartup
            in_addr v4{};
            if (!PrimaryIpv4(v4)) inet_pton(AF_INET, "127.0.0.1", &v4);
            return Mapped(v4);
        }();
        return ip;
    }

    std::array<uint8_t, 16> CurrentInterfaceIp()
    {
        in_addr v4{};
        return PrimaryIpv4(v4) ? Mapped(v4) : InterfaceIp();
    }

    std::vector<uint8_t> Snapshot::Serialize() const
    {
        std::vector<uint8_t> b(kSnapshotMagic, kSnapshotMagic + 4);
        b.push_back(eosUser.size() == 32 ? 2 : 1); // version
        b.push_back(0); // reserved
        Put16(b, gamePort);
        Put16(b, controlPort);
        b.insert(b.end(), ip.begin(), ip.end());
        for (int shift = 56; shift >= 0; shift -= 8) b.push_back(static_cast<uint8_t>(deviceId >> shift));
        if (eosUser.size() == 32)
            for (size_t i = 0; i < 32; i += 2) b.push_back(static_cast<uint8_t>(std::stoul(eosUser.substr(i, 2), nullptr, 16)));
        return b;
    }

    bool Snapshot::Parse(const uint8_t* data, size_t size, Snapshot& out)
    {
        if (size < kSize || memcmp(data, kSnapshotMagic, 4) != 0 || (data[4] != 1 && data[4] != 2)) return false;
        if (data[4] == 2 && size < kSizeV2) return false;
        out.gamePort = Get16(data + 6);
        out.controlPort = Get16(data + 8);
        memcpy(out.ip.data(), data + 10, 16);
        out.deviceId = 0;
        for (size_t i = 26; i < kSize; ++i) out.deviceId = out.deviceId << 8 | data[i];
        out.eosUser.clear();
        if (data[4] == 2)
        {
            char hex[33];
            for (size_t i = 0; i < 16; ++i) sprintf_s(hex + i * 2, 3, "%02x", data[kSize + i]);
            out.eosUser = hex;
        }
        return true;
    }

    std::wstring Snapshot::Describe() const
    {
        wchar_t text[160];
        swprintf_s(text, L"%016llX @ %s game:%u ctl:%u%s%S", deviceId, IpToString(ip).c_str(), gamePort, controlPort,
            eosUser.empty() ? L"" : L" eos:", eosUser.empty() ? "" : eosUser.substr(0, 8).c_str());
        return text;
    }

    void Start(std::function<void(const Message&)> onUnsolicited)
    {
        static std::once_flag once;
        std::call_once(once, [&]
        {
            g_onUnsolicited = std::move(onUnsolicited);
            uint16_t port = Local().controlPort; // also runs WSAStartup

            // Dual-stack so IPv4 peers (as ::ffff:a.b.c.d) and IPv6 peers both work.
            g_socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            DWORD v6only = 0;
            setsockopt(g_socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&v6only), sizeof(v6only));
            sockaddr_in6 addr{};
            addr.sin6_family = AF_INET6;
            addr.sin6_port = htons(port);
            if (bind(g_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
            {
                Log(L"net: bind control port %u FAILED (%d)", port, WSAGetLastError());
                return;
            }
            Log(L"net: control channel listening on UDP %u", port);
            std::thread(ReceiveLoop).detach();
        });
    }

    const Snapshot& Local()
    {
        static Snapshot local = MakeLocal();
        return local;
    }

    void SendTo(const sockaddr_in6& to, MsgType type, uint32_t nonce, const std::vector<uint8_t>& body)
    {
        auto packet = Encode(type, nonce, body);
        sendto(g_socket, reinterpret_cast<const char*>(packet.data()), static_cast<int>(packet.size()), 0,
               reinterpret_cast<const sockaddr*>(&to), sizeof(to));
    }

    void Send(const Snapshot& to, MsgType type, uint32_t nonce, const std::vector<uint8_t>& body)
    {
        SendTo(GameSockAddr(to.ip, to.controlPort), type, nonce, body);
    }

    bool Request(const Snapshot& to, MsgType type, const std::vector<uint8_t>& body, uint32_t timeoutMs, uint32_t retryMs, Message& reply)
    {
        auto pending = std::make_shared<Pending>();
        uint32_t nonce = NewNonce();
        {
            std::lock_guard guard(g_pendingLock);
            g_pending[nonce] = pending;
        }

        auto start = std::chrono::steady_clock::now();
        for (uint32_t elapsed = 0; elapsed < timeoutMs;)
        {
            Send(to, type, nonce, body);
            if (WaitForSingleObject(pending->event.get(), (std::min)(retryMs, timeoutMs - elapsed)) == WAIT_OBJECT_0) break;
            elapsed = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
        }

        std::lock_guard guard(g_pendingLock);
        g_pending.erase(nonce);
        if (!pending->done) return false;
        reply = std::move(pending->reply);
        return true;
    }

    uint32_t NewNonce()
    {
        static std::atomic<uint32_t> next{ static_cast<uint32_t>(Random64()) };
        return next++;
    }

    sockaddr_in6 GameSockAddr(const std::array<uint8_t, 16>& ip, uint16_t port)
    {
        sockaddr_in6 a{};
        a.sin6_family = AF_INET6;
        a.sin6_port = htons(port);
        memcpy(&a.sin6_addr, ip.data(), 16);
        return a;
    }

    std::wstring IpToString(const std::array<uint8_t, 16>& ip)
    {
        static constexpr uint8_t prefix[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff };
        wchar_t text[INET6_ADDRSTRLEN] = {};
        if (memcmp(ip.data(), prefix, sizeof(prefix)) == 0)
            InetNtopW(AF_INET, &ip[12], text, INET6_ADDRSTRLEN);
        else
            InetNtopW(AF_INET6, ip.data(), text, INET6_ADDRSTRLEN);
        return text;
    }

    std::wstring Base64Encode(const std::vector<uint8_t>& data)
    {
        static constexpr wchar_t t[] = L"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        std::wstring out;
        for (size_t i = 0; i < data.size(); i += 3)
        {
            uint32_t v = data[i] << 16;
            if (i + 1 < data.size()) v |= data[i + 1] << 8;
            if (i + 2 < data.size()) v |= data[i + 2];
            out.push_back(t[v >> 18 & 63]);
            out.push_back(t[v >> 12 & 63]);
            out.push_back(i + 1 < data.size() ? t[v >> 6 & 63] : L'=');
            out.push_back(i + 2 < data.size() ? t[v & 63] : L'=');
        }
        return out;
    }

    // Accepts standard and URL-safe alphabets; ignores padding and whitespace.
    bool Base64Decode(std::wstring_view text, std::vector<uint8_t>& out)
    {
        auto value = [](wchar_t c) -> int
        {
            if (c >= L'A' && c <= L'Z') return c - L'A';
            if (c >= L'a' && c <= L'z') return c - L'a' + 26;
            if (c >= L'0' && c <= L'9') return c - L'0' + 52;
            if (c == L'+' || c == L'-') return 62;
            if (c == L'/' || c == L'_') return 63;
            return -1;
        };
        out.clear();
        uint32_t acc = 0;
        int bits = 0;
        for (wchar_t c : text)
        {
            if (c == L'=' || c == L' ' || c == L'\r' || c == L'\n') continue;
            int v = value(c);
            if (v < 0) return false;
            acc = acc << 6 | static_cast<uint32_t>(v);
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                out.push_back(static_cast<uint8_t>(acc >> bits));
            }
        }
        return true;
    }
}
