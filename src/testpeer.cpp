// In-process fake second player, for testing the join path on one PC (pdnet.ini: test_peer=on).
//
// It talks to the game exactly as a remote pdnet PC would, but from its own sockets inside this process, so
// AppContainer loopback rules don't get in the way:
//   1. Ping the game's control port (QoS path).
//   2. CONNECT with its own snapshot -> the game gets InboundEndpointPairCreated and creates a mesh connection.
//   3. HELLO on the game port -> the game should answer with its own HELLO (the mesh handshake).
//   4. Heartbeats for a while, ACKing anything the game wants acknowledged, then DELETE to disconnect cleanly.
//
// Game packet format (PDUWP.exe, MeshPacketManager; little-endian):
//   seq u16 | type u8 | length u16 (whole message) | flags u16 (0x8000 = wants ACK) | body
//   type 1 heartbeat, 2 hello (responding u8, name length u16 in chars, UTF-16 name), 3 chat, 4 ack (seq = acked seq),
//   0x40+ game messages. Several messages can share one datagram.
#include "pch.h"
#include "testpeer.h"
#include "net.h"
#include "log.h"
#include <thread>

namespace pdnet
{
    namespace
    {
        constexpr uint16_t kCtlPort = 43212;
        constexpr uint16_t kGamePort = 43213;
        constexpr wchar_t kName[] = L"pdnet-testpeer";
        constexpr uint32_t kHeartbeatSeconds = 20;

        std::atomic<bool> g_stop{ false };
        std::atomic<int> g_hellos{ 0 }, g_heartbeats{ 0 }, g_other{ 0 };
        std::atomic<uint16_t> g_seq{ 1 };

        SOCKET OpenSocket(uint16_t port)
        {
            SOCKET s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
            DWORD off = 0, timeout = 1000;
            setsockopt(s, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));
            setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
            sockaddr_in6 a{};
            a.sin6_family = AF_INET6;
            a.sin6_port = htons(port);
            if (bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a)) != 0)
            {
                Log(L"testpeer: bind UDP %u FAILED (%d)", port, WSAGetLastError());
                closesocket(s);
                return INVALID_SOCKET;
            }
            return s;
        }

        void SendRaw(SOCKET s, const sockaddr_in6& to, const std::vector<uint8_t>& data)
        {
            sendto(s, reinterpret_cast<const char*>(data.data()), static_cast<int>(data.size()), 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
        }

        std::vector<uint8_t> Control(net::MsgType type, uint32_t nonce, const std::vector<uint8_t>& body)
        {
            std::vector<uint8_t> p = { 'P', 'D', 'N', 'C', static_cast<uint8_t>(type) };
            auto n = reinterpret_cast<const uint8_t*>(&nonce);
            p.insert(p.end(), n, n + 4);
            p.insert(p.end(), body.begin(), body.end());
            return p;
        }

        // Sends a control request and waits for a reply of `want` with the same nonce.
        bool ControlRequest(SOCKET s, const sockaddr_in6& to, net::MsgType type, const std::vector<uint8_t>& body, net::MsgType want, std::vector<uint8_t>& reply, uint32_t& rttMs)
        {
            for (int attempt = 0; attempt < 5; ++attempt)
            {
                uint32_t nonce = net::NewNonce();
                auto start = std::chrono::steady_clock::now();
                SendRaw(s, to, Control(type, nonce, body));
                uint8_t buf[2048];
                for (;;)
                {
                    int n = recv(s, reinterpret_cast<char*>(buf), sizeof(buf), 0);
                    if (n == SOCKET_ERROR) break; // timeout: retry
                    uint32_t got;
                    if (n < 9 || memcmp(buf, "PDNC", 4) != 0) continue;
                    memcpy(&got, buf + 5, 4);
                    if (buf[4] != static_cast<uint8_t>(want) || got != nonce) continue;
                    rttMs = static_cast<uint32_t>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
                    reply.assign(buf + 9, buf + n);
                    return true;
                }
            }
            return false;
        }

        std::vector<uint8_t> GameMessage(uint8_t type, uint16_t flags, const std::vector<uint8_t>& body, uint16_t seq = 0)
        {
            if (!seq) seq = g_seq++;
            uint16_t length = static_cast<uint16_t>(7 + body.size());
            std::vector<uint8_t> m(7);
            memcpy(&m[0], &seq, 2);
            m[2] = type;
            memcpy(&m[3], &length, 2);
            memcpy(&m[5], &flags, 2);
            m.insert(m.end(), body.begin(), body.end());
            return m;
        }

        std::vector<uint8_t> Hello(bool responding)
        {
            std::wstring name = kName;
            uint16_t chars = static_cast<uint16_t>(name.size());
            std::vector<uint8_t> body = { static_cast<uint8_t>(responding) };
            body.push_back(static_cast<uint8_t>(chars & 0xff));
            body.push_back(static_cast<uint8_t>(chars >> 8));
            auto p = reinterpret_cast<const uint8_t*>(name.data());
            body.insert(body.end(), p, p + chars * 2);
            return GameMessage(2, 0, body);
        }

        // Logs every message the game sends to our game port and ACKs the ones that ask for it.
        void GameReceiver(SOCKET s, sockaddr_in6 game)
        {
            uint8_t buf[4096];
            while (!g_stop)
            {
                sockaddr_in6 from{};
                int fromLen = sizeof(from);
                int n = recvfrom(s, reinterpret_cast<char*>(buf), sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
                if (n == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSAETIMEDOUT || err == WSAECONNRESET) continue;
                    if (!g_stop) Log(L"testpeer: game socket receive failed (%d)", err);
                    return;
                }
                for (int off = 0; off + 7 <= n;)
                {
                    uint16_t seq, length, flags;
                    memcpy(&seq, buf + off, 2);
                    memcpy(&length, buf + off + 3, 2);
                    memcpy(&flags, buf + off + 5, 2);
                    uint8_t type = buf[off + 2];
                    if (length < 7 || off + length > n)
                    {
                        Log(L"testpeer: malformed game packet (%d bytes, message length %u at offset %d)", n, length, off);
                        break;
                    }
                    if (type == 2 && length >= 10)
                    {
                        uint16_t chars;
                        memcpy(&chars, buf + off + 8, 2);
                        std::wstring name(reinterpret_cast<const wchar_t*>(buf + off + 10), (std::min)(static_cast<size_t>(chars), static_cast<size_t>((length - 10) / 2)));
                        Log(L"testpeer: <- HELLO seq %u from game, name '%s', responding=%u", seq, name.c_str(), buf[off + 7]);
                        if (!buf[off + 7]) SendRaw(s, game, Hello(true));
                        ++g_hellos;
                    }
                    else if (type == 1)
                    {
                        if (g_heartbeats++ < 3) Log(L"testpeer: <- heartbeat seq %u (%u bytes)", seq, length);
                    }
                    else
                    {
                        if (g_other++ < 20) Log(L"testpeer: <- type 0x%02X seq %u, %u bytes, flags 0x%04X", type, seq, length, flags);
                    }
                    if (flags & 0x8000) SendRaw(s, game, GameMessage(4, 0, {}, seq)); // ACK echoes their seq
                    off += length;
                }
            }
        }

        // Answers anything the game sends to our control port (QoS pings, deletes).
        void ControlReceiver(SOCKET s, std::vector<uint8_t> snapshot)
        {
            uint8_t buf[2048];
            while (!g_stop)
            {
                sockaddr_in6 from{};
                int fromLen = sizeof(from);
                int n = recvfrom(s, reinterpret_cast<char*>(buf), sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
                if (n == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSAETIMEDOUT || err == WSAECONNRESET) continue;
                    return;
                }
                if (n < 9 || memcmp(buf, "PDNC", 4) != 0) continue;
                uint32_t nonce;
                memcpy(&nonce, buf + 5, 4);
                auto type = static_cast<net::MsgType>(buf[4]);
                if (type == net::MsgType::Ping)
                {
                    SendRaw(s, from, Control(net::MsgType::Pong, nonce, { 0, 0 }));
                }
                else if (type == net::MsgType::Connect)
                {
                    Log(L"testpeer: <- CONNECT from the game (it is connecting back to us)");
                    SendRaw(s, from, Control(net::MsgType::Ack, nonce, snapshot));
                }
                else
                {
                    Log(L"testpeer: <- control message type %u from the game", buf[4]);
                }
            }
        }

        void Run(std::wstring templateName)
        {
            Sleep(3000);
            Log(L"testpeer: starting (template '%s'): control UDP %u, game UDP %u", templateName.c_str(), kCtlPort, kGamePort);

            SOCKET ctl = OpenSocket(kCtlPort);
            SOCKET game = OpenSocket(kGamePort);
            if (ctl == INVALID_SOCKET || game == INVALID_SOCKET)
            {
                if (ctl != INVALID_SOCKET) closesocket(ctl);
                if (game != INVALID_SOCKET) closesocket(game);
                return;
            }

            const net::Snapshot& host = net::Local();
            net::Snapshot me = host;
            me.deviceId = host.deviceId ^ 0x5445535450454552ull; // "TESTPEER"
            me.gamePort = kGamePort;
            me.controlPort = kCtlPort;
            auto hostCtl = net::GameSockAddr(host.ip, host.controlPort);
            auto hostGame = net::GameSockAddr(host.ip, host.gamePort);
            Log(L"testpeer: I am %s; the game is %s", me.Describe().c_str(), host.Describe().c_str());

            std::vector<uint8_t> reply;
            uint32_t rtt = 0;
            bool pinged = ControlRequest(ctl, hostCtl, net::MsgType::Ping, {}, net::MsgType::Pong, reply, rtt);
            Log(pinged ? L"testpeer: 1/4 PASS ping -> pong in %u ms (%zu-byte QoS payload)" : L"testpeer: 1/4 FAIL no pong from the game's control port", rtt, reply.size() >= 2 ? reply.size() - 2 : 0);

            std::string name = winrt::to_string(templateName);
            auto body = me.Serialize();
            body.push_back(static_cast<uint8_t>(name.size()));
            body.insert(body.end(), name.begin(), name.end());
            net::Snapshot acked;
            bool connected = ControlRequest(ctl, hostCtl, net::MsgType::Connect, body, net::MsgType::Ack, reply, rtt) &&
                             net::Snapshot::Parse(reply.data(), reply.size(), acked);
            if (connected) Log(L"testpeer: 2/4 PASS connect -> ack in %u ms from %s", rtt, acked.Describe().c_str());
            else Log(L"testpeer: 2/4 FAIL no ack to CONNECT");

            if (connected)
            {
                std::thread gameThread(GameReceiver, game, hostGame);
                std::thread ctlThread(ControlReceiver, ctl, me.Serialize());
                Sleep(500); // give the game time to handle InboundEndpointPairCreated before the first packet

                SendRaw(game, hostGame, Hello(false));
                Log(L"testpeer: -> HELLO as '%s'", kName);
                for (int i = 0; i < 50 && !g_hellos; ++i) Sleep(100);
                Log(g_hellos ? L"testpeer: 3/4 PASS the game answered our HELLO (mesh handshake complete)"
                             : L"testpeer: 3/4 FAIL no HELLO back from the game within 5 s");

                for (uint32_t i = 0; i < kHeartbeatSeconds; ++i)
                {
                    SendRaw(game, hostGame, GameMessage(1, 0, {}));
                    Sleep(1000);
                }
                Log(L"testpeer: 4/4 %s %d heartbeat(s), %d other message(s) from the game over %u s",
                    g_heartbeats ? L"PASS" : L"FAIL", g_heartbeats.load(), g_other.load(), kHeartbeatSeconds);

                SendRaw(ctl, hostCtl, Control(net::MsgType::Delete, net::NewNonce(), body));
                Log(L"testpeer: -> DELETE (disconnecting)");
                Sleep(1500);
                g_stop = true;
                gameThread.join();
                ctlThread.join();
            }
            closesocket(ctl);
            closesocket(game);
            Log(L"testpeer: finished");
        }
    }

    void StartTestPeer(const std::wstring& templateName)
    {
        static std::once_flag once;
        std::call_once(once, [&] { std::thread(Run, templateName).detach(); });
    }
}
