#include "pch.h"
#include "p2p.h"
#include "config.h"
#include "eos.h"
#include "log.h"

#ifdef PDNET_NO_EOS

namespace pdnet::p2p
{
    bool Enabled() { return false; }
    void Start(std::function<void(const net::Message&)>) {}
    void SendControl(const std::string&, net::MsgType, uint32_t, const std::vector<uint8_t>&) {}
    bool Request(const std::string&, net::MsgType, const std::vector<uint8_t>&, uint32_t, net::Message&) { return false; }
    int Ping(const std::string&, uint32_t, std::vector<uint8_t>*) { return -1; }
    uint16_t OpenLink(const std::string&) { return 0; }
    void CloseLink(const std::string&) {}
}

#else

#include "eossdk.h"
#include <condition_variable>
#include <deque>
#include <thread>

namespace pdnet
{
    std::vector<uint8_t> PublishedPayload(); // sda.cpp: the QoS payload pongs carry
}

namespace pdnet::p2p
{
    namespace
    {
        using namespace eossdk;
        constexpr uint8_t kControl = 0, kGame = 1, kPing = 2;

        struct Outgoing
        {
            std::string peer;
            uint8_t channel;
            std::vector<uint8_t> data;
        };

        struct Pending
        {
            winrt::handle event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
            net::Message reply;
        };

        // A peer reached over EOS: the in-process UDP socket the game talks to, plus traffic and ping statistics.
        struct Link
        {
            std::string peer;
            SOCKET socket = INVALID_SOCKET;
            uint16_t port = 0;
            std::atomic<bool> closing{ false };
            std::atomic<uint64_t> outPackets{ 0 }, outBytes{ 0 }, inPackets{ 0 }, inBytes{ 0 }, oversize{ 0 };
            // ping window (monitor thread only)
            std::vector<int> rtts;
            int pingsSent = 0;
        };

        std::function<void(const net::Message&)> g_onControl;

        std::mutex g_lock; // links, pending, path types
        std::map<std::string, std::shared_ptr<Link>> g_links;
        std::map<uint32_t, std::shared_ptr<Pending>> g_pending;
        std::map<std::string, std::wstring> g_paths; // peer -> "direct" / "relayed"

        std::mutex g_outLock;
        std::deque<Outgoing> g_out;

        std::mutex g_dispatchLock;
        std::condition_variable g_dispatchWake;
        std::deque<net::Message> g_dispatch;

        std::string Narrow(const Api& api, EOS_ProductUserId id)
        {
            char buf[EOS_PRODUCTUSERID_MAX_LENGTH + 1] = {};
            int32_t len = sizeof(buf);
            return id && api.EOS_ProductUserId_ToString(id, buf, &len) == EOS_EResult::EOS_Success ? std::string(buf) : std::string();
        }

        const EOS_P2P_SocketId* Socket()
        {
            static EOS_P2P_SocketId id = []
            {
                EOS_P2P_SocketId s{};
                s.ApiVersion = EOS_P2P_SOCKETID_API_LATEST;
                strcpy_s(s.SocketName, "PDNET");
                return s;
            }();
            return &id;
        }

        void Queue(Outgoing out)
        {
            {
                std::lock_guard guard(g_outLock);
                g_out.push_back(std::move(out));
            }
            eos::Wake();
        }

        sockaddr_in6 GameAddress()
        {
            return net::GameSockAddr(net::InterfaceIp(), GetConfig().gamePort);
        }

        // Game -> link socket -> EOS.
        void LinkReader(std::shared_ptr<Link> link)
        {
            std::vector<uint8_t> buf(64 * 1024);
            while (!link->closing)
            {
                sockaddr_in6 from{};
                int fromLen = sizeof(from);
                int n = recvfrom(link->socket, reinterpret_cast<char*>(buf.data()), static_cast<int>(buf.size()), 0, reinterpret_cast<sockaddr*>(&from), &fromLen);
                if (n == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSAETIMEDOUT || err == WSAECONNRESET) continue;
                    if (!link->closing) Log(L"p2p: link to %S stopped receiving (%d)", link->peer.substr(0, 8).c_str(), err);
                    return;
                }
                if (n > EOS_P2P_MAX_PACKET_SIZE)
                {
                    if (link->oversize++ < 5) Log(L"p2p: game datagram of %d bytes is over the EOS limit (%d), dropped", n, EOS_P2P_MAX_PACKET_SIZE);
                    continue;
                }
                link->outPackets++;
                link->outBytes += n;
                Queue({ link->peer, kGame, std::vector<uint8_t>(buf.begin(), buf.begin() + n) });
            }
        }

        void Complete(net::Message&& m)
        {
            std::shared_ptr<Pending> pending;
            {
                std::lock_guard guard(g_lock);
                if (auto it = g_pending.find(m.nonce); it != g_pending.end()) pending = it->second;
                if (pending) pending->reply = std::move(m);
            }
            if (pending) SetEvent(pending->event.get());
            else if (GetConfig().p2pDebug) Log(L"p2p-debug: reply nonce %u matches no waiting request (too late?)", m.nonce);
        }

        void Dispatch(net::Message&& m)
        {
            {
                std::lock_guard guard(g_dispatchLock);
                g_dispatch.push_back(std::move(m));
            }
            g_dispatchWake.notify_one();
        }

        void Dispatcher()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            for (;;)
            {
                net::Message m;
                {
                    std::unique_lock guard(g_dispatchLock);
                    g_dispatchWake.wait(guard, [] { return !g_dispatch.empty(); });
                    m = std::move(g_dispatch.front());
                    g_dispatch.pop_front();
                }
                try { if (g_onControl) g_onControl(m); }
                catch (...) { Log(L"p2p: control handler threw 0x%08X", winrt::to_hresult()); }
            }
        }

        // ---- EOS thread ----

        void EOS_CALL OnRequest(const EOS_P2P_OnIncomingConnectionRequestInfo* info)
        {
            const Api& api = *Load(ModuleDir());
            EOS_P2P_AcceptConnectionOptions o{ EOS_P2P_ACCEPTCONNECTION_API_LATEST, info->LocalUserId, info->RemoteUserId, Socket() };
            EOS_EResult r = api.EOS_P2P_AcceptConnection(api.EOS_Platform_GetP2PInterface(static_cast<EOS_HPlatform>(info->ClientData)), &o);
            Log(L"p2p: connection request from %S -> %s", Narrow(api, info->RemoteUserId).substr(0, 8).c_str(), r == EOS_EResult::EOS_Success ? L"accepted" : Result(api, r).c_str());
        }

        void EOS_CALL OnEstablished(const EOS_P2P_OnPeerConnectionEstablishedInfo* info)
        {
            const Api& api = *Load(ModuleDir());
            std::string peer = Narrow(api, info->RemoteUserId);
            const wchar_t* path = info->NetworkType == EOS_ENetworkConnectionType::EOS_NCT_DirectConnection ? L"direct"
                                : info->NetworkType == EOS_ENetworkConnectionType::EOS_NCT_RelayedConnection ? L"relayed (Epic relay)" : L"none";
            {
                std::lock_guard guard(g_lock);
                g_paths[peer] = path;
            }
            Log(L"p2p: %s to %S: %s", info->ConnectionType == EOS_EConnectionEstablishedType::EOS_CET_Reconnection ? L"reconnected" : L"connected", peer.substr(0, 8).c_str(), path);
        }

        void EOS_CALL OnInterrupted(const EOS_P2P_OnPeerConnectionInterruptedInfo* info)
        {
            Log(L"p2p: connection to %S interrupted (EOS is trying to restore it)", Narrow(*Load(ModuleDir()), info->RemoteUserId).substr(0, 8).c_str());
        }

        void EOS_CALL OnClosed(const EOS_P2P_OnRemoteConnectionClosedInfo* info)
        {
            const Api& api = *Load(ModuleDir());
            std::string peer = Narrow(api, info->RemoteUserId);
            {
                std::lock_guard guard(g_lock);
                g_paths.erase(peer);
            }
            Log(L"p2p: connection to %S closed (reason %d)", peer.substr(0, 8).c_str(), static_cast<int>(info->Reason));
        }

        void Init(const Api& api, EOS_HPlatform platform, EOS_HP2P p2p, EOS_ProductUserId me)
        {
            EOS_P2P_AddNotifyPeerConnectionRequestOptions req{ EOS_P2P_ADDNOTIFYPEERCONNECTIONREQUEST_API_LATEST, me, Socket() };
            api.EOS_P2P_AddNotifyPeerConnectionRequest(p2p, &req, platform, OnRequest);
            EOS_P2P_AddNotifyPeerConnectionEstablishedOptions est{ EOS_P2P_ADDNOTIFYPEERCONNECTIONESTABLISHED_API_LATEST, me, Socket() };
            api.EOS_P2P_AddNotifyPeerConnectionEstablished(p2p, &est, nullptr, OnEstablished);
            EOS_P2P_AddNotifyPeerConnectionInterruptedOptions intr{ EOS_P2P_ADDNOTIFYPEERCONNECTIONINTERRUPTED_API_LATEST, me, Socket() };
            api.EOS_P2P_AddNotifyPeerConnectionInterrupted(p2p, &intr, nullptr, OnInterrupted);
            EOS_P2P_AddNotifyPeerConnectionClosedOptions closed{ EOS_P2P_ADDNOTIFYPEERCONNECTIONCLOSED_API_LATEST, me, Socket() };
            api.EOS_P2P_AddNotifyPeerConnectionClosed(p2p, &closed, nullptr, OnClosed);

            const std::wstring& relay = GetConfig().p2pRelay;
            EOS_P2P_SetRelayControlOptions rc{ EOS_P2P_SETRELAYCONTROL_API_LATEST,
                _wcsicmp(relay.c_str(), L"force") == 0 ? EOS_ERelayControl::EOS_RC_ForceRelays
                : _wcsicmp(relay.c_str(), L"off") == 0 ? EOS_ERelayControl::EOS_RC_NoRelays : EOS_ERelayControl::EOS_RC_AllowRelays };
            api.EOS_P2P_SetRelayControl(p2p, &rc);

            EOS_P2P_GetNATTypeOptions nat{ EOS_P2P_GETNATTYPE_API_LATEST };
            EOS_ENATType type = EOS_ENATType::EOS_NAT_Unknown;
            api.EOS_P2P_GetNATType(p2p, &nat, &type);
            static const wchar_t* names[] = { L"unknown (not queried yet)", L"open", L"moderate", L"strict" };
            Log(L"p2p: ready (relays %s, NAT %s)", relay.c_str(), names[static_cast<int>(type) & 3]);
        }

        void Pump(void* platformPtr)
        {
            if (!eos::SignedIn()) return;
            const Api* sdk = Load(ModuleDir());
            if (!sdk) return;
            const Api& api = *sdk;
            auto platform = static_cast<EOS_HPlatform>(platformPtr);
            EOS_HP2P p2p = api.EOS_Platform_GetP2PInterface(platform);

            static std::string meText;
            static EOS_ProductUserId me = nullptr;
            static bool initialized = false;
            std::string current = winrt::to_string(eos::LocalUserId());
            if (current != meText)
            {
                meText = current;
                me = api.EOS_ProductUserId_FromString(meText.c_str());
                initialized = false;
            }
            if (!me) return;
            if (!initialized)
            {
                Init(api, platform, p2p, me);
                initialized = true;
            }

            // Send everything queued.
            std::deque<Outgoing> out;
            {
                std::lock_guard guard(g_outLock);
                out.swap(g_out);
            }
            for (auto& o : out)
            {
                EOS_P2P_SendPacketOptions s{};
                s.ApiVersion = EOS_P2P_SENDPACKET_API_LATEST;
                s.LocalUserId = me;
                s.RemoteUserId = api.EOS_ProductUserId_FromString(o.peer.c_str());
                s.SocketId = Socket();
                s.Channel = o.channel;
                s.DataLengthBytes = static_cast<uint32_t>(o.data.size());
                s.Data = o.data.data();
                s.bAllowDelayedDelivery = EOS_TRUE; // queue while the connection is being made
                s.Reliability = o.channel == kControl ? EOS_EPacketReliability::EOS_PR_ReliableOrdered : EOS_EPacketReliability::EOS_PR_UnreliableUnordered;
                s.bDisableAutoAcceptConnection = EOS_FALSE;
                EOS_EResult r = api.EOS_P2P_SendPacket(p2p, &s);
                static int failures = 0;
                if (r != EOS_EResult::EOS_Success && failures++ < 20) Log(L"p2p: send to %S failed: %s", o.peer.substr(0, 8).c_str(), Result(api, r).c_str());
                net::Message sent;
                if (GetConfig().p2pDebug && o.channel == kControl && net::Decode(o.data.data(), o.data.size(), sent))
                    Log(L"p2p-debug: sent type %d nonce %u (%zu bytes) to %S: %s", static_cast<int>(sent.type), sent.nonce, o.data.size(), o.peer.substr(0, 8).c_str(), Result(api, r).c_str());
            }

            // Receive everything waiting.
            uint8_t buf[EOS_P2P_MAX_PACKET_SIZE];
            for (int i = 0; i < 512; ++i)
            {
                EOS_P2P_GetNextReceivedPacketSizeOptions so{ EOS_P2P_GETNEXTRECEIVEDPACKETSIZE_API_LATEST, me, nullptr };
                uint32_t size = 0;
                if (api.EOS_P2P_GetNextReceivedPacketSize(p2p, &so, &size) != EOS_EResult::EOS_Success) break;
                EOS_P2P_ReceivePacketOptions ro{ EOS_P2P_RECEIVEPACKET_API_LATEST, me, sizeof(buf), nullptr };
                EOS_ProductUserId from = nullptr;
                EOS_P2P_SocketId socket{};
                uint8_t channel = 0;
                uint32_t bytes = 0;
                if (api.EOS_P2P_ReceivePacket(p2p, &ro, &from, &socket, &channel, buf, &bytes) != EOS_EResult::EOS_Success) break;
                std::string peer = Narrow(api, from);

                if (channel == kGame)
                {
                    std::shared_ptr<Link> link;
                    {
                        std::lock_guard guard(g_lock);
                        if (auto it = g_links.find(peer); it != g_links.end()) link = it->second;
                    }
                    if (!link) continue; // no endpoint pair for this peer (yet)
                    link->inPackets++;
                    link->inBytes += bytes;
                    auto to = GameAddress();
                    sendto(link->socket, reinterpret_cast<const char*>(buf), static_cast<int>(bytes), 0, reinterpret_cast<const sockaddr*>(&to), sizeof(to));
                    continue;
                }

                net::Message m;
                if (!net::Decode(buf, bytes, m))
                {
                    if (GetConfig().p2pDebug) Log(L"p2p-debug: %u bytes on channel %u from %S did not decode", bytes, channel, peer.substr(0, 8).c_str());
                    continue;
                }
                m.peer = peer;
                if (GetConfig().p2pDebug && channel == kControl)
                    Log(L"p2p-debug: received type %d nonce %u (%u bytes) from %S", static_cast<int>(m.type), m.nonce, bytes, peer.substr(0, 8).c_str());
                if (channel == kPing && m.type == net::MsgType::Ping)
                {
                    auto payload = PublishedPayload();
                    std::vector<uint8_t> body{ static_cast<uint8_t>(payload.size() >> 8), static_cast<uint8_t>(payload.size() & 0xff) };
                    body.insert(body.end(), payload.begin(), payload.end());
                    Queue({ peer, kPing, net::Encode(net::MsgType::Pong, m.nonce, body) });
                }
                else if (m.type == net::MsgType::Ack || m.type == net::MsgType::Pong)
                {
                    Complete(std::move(m));
                }
                else
                {
                    Dispatch(std::move(m));
                }
            }
        }

        std::shared_ptr<Pending> AddPending(uint32_t nonce)
        {
            auto p = std::make_shared<Pending>();
            std::lock_guard guard(g_lock);
            g_pending[nonce] = p;
            return p;
        }

        void RemovePending(uint32_t nonce)
        {
            std::lock_guard guard(g_lock);
            g_pending.erase(nonce);
        }

        // Pings every link every 2 s and logs its quality every 10 s.
        void Monitor()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            for (int tick = 1;; ++tick)
            {
                Sleep(2000);
                std::vector<std::shared_ptr<Link>> links;
                {
                    std::lock_guard guard(g_lock);
                    for (auto& [peer, link] : g_links) links.push_back(link);
                }
                for (auto& link : links)
                {
                    link->pingsSent++;
                    int rtt = Ping(link->peer, 1500);
                    if (rtt >= 0) link->rtts.push_back(rtt);
                }
                if (tick % 5) continue;
                for (auto& link : links)
                {
                    std::wstring path;
                    {
                        std::lock_guard guard(g_lock);
                        auto it = g_paths.find(link->peer);
                        path = it == g_paths.end() ? L"not connected" : it->second;
                    }
                    int lo = 0, hi = 0, avg = 0, jitter = 0;
                    if (!link->rtts.empty())
                    {
                        lo = *std::min_element(link->rtts.begin(), link->rtts.end());
                        hi = *std::max_element(link->rtts.begin(), link->rtts.end());
                        long long sum = 0;
                        for (int r : link->rtts) sum += r;
                        avg = static_cast<int>(sum / link->rtts.size());
                        for (size_t i = 1; i < link->rtts.size(); ++i) jitter += abs(link->rtts[i] - link->rtts[i - 1]);
                        if (link->rtts.size() > 1) jitter /= static_cast<int>(link->rtts.size() - 1);
                    }
                    int lost = link->pingsSent - static_cast<int>(link->rtts.size());
                    Log(L"p2p: link %S %s | ping %d ms (%d-%d) jitter %d ms, %d/%d pings lost | game out %llu pkt %llu KB, in %llu pkt %llu KB",
                        link->peer.substr(0, 8).c_str(), path.c_str(), avg, lo, hi, jitter, lost, link->pingsSent,
                        link->outPackets.exchange(0), link->outBytes.exchange(0) / 1024, link->inPackets.exchange(0), link->inBytes.exchange(0) / 1024);
                    link->rtts.clear();
                    link->pingsSent = 0;
                }
            }
        }
    }

    bool Enabled() { return GetConfig().p2p && eos::SignedIn(); }

    void Start(std::function<void(const net::Message&)> onControl)
    {
        static std::once_flag once;
        std::call_once(once, [&]
        {
            g_onControl = std::move(onControl);
            net::InterfaceIp(); // WSAStartup
            std::thread(Dispatcher).detach();
            std::thread(Monitor).detach();
            eos::SetPump(Pump);
            Log(L"p2p: starting (game datagrams via EOS P2P; links on %s)", net::IpToString(net::InterfaceIp()).c_str());
        });
    }

    void SendControl(const std::string& peer, net::MsgType type, uint32_t nonce, const std::vector<uint8_t>& body)
    {
        Queue({ peer, kControl, net::Encode(type, nonce, body) });
    }

    bool Request(const std::string& peer, net::MsgType type, const std::vector<uint8_t>& body, uint32_t timeoutMs, net::Message& reply)
    {
        uint32_t nonce = net::NewNonce();
        auto pending = AddPending(nonce);
        // Sent again every 2 s until answered: a request sent while EOS is still setting up the connection can be
        // lost (joins then hung on "joining"). The other side answers each copy; the extra answers are ignored.
        constexpr DWORD kResendMs = 2000;
        bool ok = false;
        for (DWORD waited = 0, sends = 1; !ok && waited < timeoutMs; ++sends)
        {
            SendControl(peer, type, nonce, body);
            DWORD slice = (std::min)(kResendMs, timeoutMs - waited);
            ok = WaitForSingleObject(pending->event.get(), slice) == WAIT_OBJECT_0;
            waited += slice;
            if (!ok && waited < timeoutMs && sends == 1) Log(L"p2p: no answer from %S yet - sending the request again every %lu s", peer.substr(0, 8).c_str(), kResendMs / 1000);
        }
        RemovePending(nonce);
        if (ok)
        {
            std::lock_guard guard(g_lock);
            reply = pending->reply;
        }
        return ok;
    }

    int Ping(const std::string& peer, uint32_t timeoutMs, std::vector<uint8_t>* payload)
    {
        uint32_t nonce = net::NewNonce();
        auto pending = AddPending(nonce);
        auto start = std::chrono::steady_clock::now();
        Queue({ peer, kPing, net::Encode(net::MsgType::Ping, nonce, {}) });
        bool ok = WaitForSingleObject(pending->event.get(), timeoutMs) == WAIT_OBJECT_0;
        RemovePending(nonce);
        if (!ok) return -1;
        if (payload)
        {
            std::lock_guard guard(g_lock);
            auto& body = pending->reply.body;
            if (body.size() >= 2)
            {
                size_t len = (std::min)(static_cast<size_t>(body[0] << 8 | body[1]), body.size() - 2);
                payload->assign(body.begin() + 2, body.begin() + 2 + len);
            }
        }
        return static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - start).count());
    }

    uint16_t OpenLink(const std::string& peer)
    {
        std::lock_guard guard(g_lock);
        if (auto it = g_links.find(peer); it != g_links.end()) return it->second->port;

        auto link = std::make_shared<Link>();
        link->peer = peer;
        link->socket = socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
        DWORD off = 0, timeout = 1000;
        setsockopt(link->socket, IPPROTO_IPV6, IPV6_V6ONLY, reinterpret_cast<const char*>(&off), sizeof(off));
        setsockopt(link->socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
        sockaddr_in6 any{};
        any.sin6_family = AF_INET6; // port 0: Windows picks a free one
        sockaddr_in6 bound{};
        int len = sizeof(bound);
        if (link->socket == INVALID_SOCKET || bind(link->socket, reinterpret_cast<sockaddr*>(&any), sizeof(any)) != 0 ||
            getsockname(link->socket, reinterpret_cast<sockaddr*>(&bound), &len) != 0)
        {
            Log(L"p2p: could not open a link socket for %S (%d)", peer.substr(0, 8).c_str(), WSAGetLastError());
            if (link->socket != INVALID_SOCKET) closesocket(link->socket);
            return 0;
        }
        link->port = ntohs(bound.sin6_port);
        g_links[peer] = link;
        std::thread(LinkReader, link).detach();
        Log(L"p2p: link to %S on local port %u", peer.substr(0, 8).c_str(), link->port);
        return link->port;
    }

    void CloseLink(const std::string& peer)
    {
        std::shared_ptr<Link> link;
        {
            std::lock_guard guard(g_lock);
            if (auto it = g_links.find(peer); it != g_links.end())
            {
                link = it->second;
                g_links.erase(it);
            }
        }
        if (!link) return;
        link->closing = true;
        closesocket(link->socket);
        Log(L"p2p: link to %S closed", peer.substr(0, 8).c_str());
    }
}

#endif
