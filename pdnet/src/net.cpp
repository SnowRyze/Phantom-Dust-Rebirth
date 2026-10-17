#include "pch.h"
#include "net.h"
#include "config.h"
#include "log.h"
#include "upnp.h"
#include <bcrypt.h>
#include <thread>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "bcrypt.lib")

namespace pdnet::net
{
    namespace
    {
        constexpr uint8_t kSnapshotMagic[4] = { 'P', 'D', 'N', '2' };
        constexpr uint8_t kMsgMagic[4] = { 'P', 'D', 'N', 'C' };
        constexpr size_t kHeader = 4 + 1 + 4;
        constexpr uint32_t kStunCookie = 0x2112A442;

        std::function<void(const Message&)> g_onUnsolicited;

        // The game's UDP socket (seen in its receive loop) and our fallback socket for use before it exists.
        std::atomic<SOCKET> g_gameSocket{ INVALID_SOCKET };
        SOCKET g_fallback = INVALID_SOCKET;
        winrt::handle g_gameSocketReady{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };

        std::mutex g_localLock;
        Snapshot g_local;
        std::vector<std::function<void()>> g_localChanged;

        struct Pending
        {
            winrt::handle event{ CreateEventW(nullptr, TRUE, FALSE, nullptr) };
            Message reply{};
            std::vector<uint8_t> raw;
        };
        std::mutex g_pendingLock;
        std::map<uint32_t, std::shared_ptr<Pending>> g_pending;     // pdnet replies by nonce
        std::map<uint32_t, std::shared_ptr<Pending>> g_stunPending; // STUN replies by transaction id prefix

        uint64_t Random64()
        {
            uint64_t v = 0;
            BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(&v), sizeof(v), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
            return v;
        }

        void Put16(std::vector<uint8_t>& b, uint16_t v) { b.push_back(v >> 8); b.push_back(v & 0xff); }
        uint16_t Get16(const uint8_t* p) { return static_cast<uint16_t>(p[0] << 8 | p[1]); }

        Endpoint V4(const in_addr& a, uint16_t port)
        {
            Endpoint e;
            e.ip[10] = e.ip[11] = 0xff;
            memcpy(&e.ip[12], &a, 4);
            e.port = port;
            return e;
        }

        // Source address Windows would use to reach `probe` (no packets are sent).
        bool RouteSource(int family, const char* probe, sockaddr_storage& out)
        {
            SOCKET s = socket(family, SOCK_DGRAM, IPPROTO_UDP);
            if (s == INVALID_SOCKET) return false;
            sockaddr_storage dst{};
            int len = family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);