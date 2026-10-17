#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

// pdnet transport: everything rides on the game's own UDP socket (one port, nothing to forward).
// Our packets start with "PDNC" and are filtered out of the game's receive loop before it sees them.
namespace pdnet::net
{
    struct Endpoint
    {
        std::array<uint8_t, 16> ip{}; // IPv6, or IPv4-mapped (::ffff:a.b.c.d)
        uint16_t port = 0;

        bool operator==(const Endpoint&) const = default;
        bool IsV4() const;
        sockaddr_in6 ToSockAddr() const;
        static Endpoint From(const sockaddr* a, int len);
        std::wstring ToString() const;
    };

    enum class CandidateKind : uint8_t
    {
        Lan = 1,      // this PC's LAN address
        Public = 2,   // public IPv4:port the game socket maps to (STUN)
        Guess = 3,    // public IPv4 with the game port, before the game socket exists
        Ipv6 = 4,     // global IPv6 (no NAT)
        Upnp = 5,     // router port mapping created via UPnP
    };

    struct Candidate
    {
        CandidateKind kind;
        Endpoint ep;
    };

    struct Snapshot
    {
        enum Flags : uint8_t { SymmetricNat = 1 };

        uint64_t deviceId = 0;
        uint8_t flags = 0;
        std::vector<Candidate> candidates;

        static constexpr size_t kMaxSize = 128;
        std::vector<uint8_t> Serialize() const;
        static bool Parse(const uint8_t* data, size_t size, Snapshot& out);
        std::wstring Describe() const;
    };

    enum class MsgType : uint8_t
    {
        Connect = 1,  // snapshot, template name -> Ack
        Ack = 2,      // snapshot
        Delete = 3,   // snapshot, template name
        Ping = 4,     // -> Pong
        Pong = 5,     // u16 payload length, payload (published QoS private payload)
        Punch = 6,    // u64 device id; opens NAT mappings, no reply
    };

    struct Message
    {
        MsgType type;
        uint32_t nonce;
        std::vector<uint8_t> body;
        Endpoint from;
        bool viaGameSocket;
    };

    // Starts discovery (LAN/IPv6 now, STUN in the background) and the fallback socket. Idempotent.
    void Start(std::function<void(const Message&)> onUnsolicited);

    // Current local snapshot (changes when STUN/UPnP finish or the game socket appears).
    Snapshot Local();
    uint64_t LocalDeviceId();
    void OnLocalChanged(std::function<void()> handler);

    // Called from the hooked WSARecvFrom. Returns true if the packet was ours (hide it from the game).
    bool InterceptGamePacket(SOCKET s, const uint8_t* data, size_t size, const sockaddr* from, int fromLen);

    // Waits up to `ms` for the game's socket to exist (it is created when online play starts).
    bool WaitForGameSocket(uint32_t ms);

    void Send(const Endpoint& to, MsgType type, uint32_t nonce, const std::vector<uint8_t>& body, bool viaGame = true);

    // Sends to every candidate of `to` repeatedly until a reply with the same nonce arrives.
    bool Request(const Snapshot& to, MsgType type, const std::vector<uint8_t>& body, uint32_t timeoutMs, uint32_t retryMs, Message& reply);

    // Sends punch packets to every candidate for a few seconds so both NATs open toward each other.
    void Punch(const Snapshot& peer);

    uint32_t NewNonce();
    std::wstring Base64Encode(const std::vector<uint8_t>& data);
    bool Base64Decode(std::wstring_view text, std::vector<uint8_t>& out);
}
