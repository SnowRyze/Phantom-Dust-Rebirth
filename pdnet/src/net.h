#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

// pdnet control channel: a small UDP protocol on the control port (default 43211) that stands in for
// Secure Device Association. Game traffic itself stays on the game's own socket (43210).
namespace pdnet::net
{
    // What a device address carries. Wire format (big-endian):
    //   v1 (34 bytes): "PDN1" | version 1 | reserved 0 | gamePort | controlPort | ip[16] | deviceId
    //   v2 (50 bytes): the same with version 2, then the EOS product user id (16 bytes = its 32 hex digits), so peers
    //                  can reach this PC through EOS P2P (network_mode=eos, see p2p.h). v1 peers keep using direct UDP.
    struct Snapshot
    {
        std::array<uint8_t, 16> ip{}; // IPv6, or IPv4-mapped (::ffff:a.b.c.d)
        uint16_t gamePort = 0;
        uint16_t controlPort = 0;
        uint64_t deviceId = 0;
        std::string eosUser; // 32 lowercase hex digits, or empty (v1)

        static constexpr size_t kSize = 34;
        static constexpr size_t kSizeV2 = 50;
        std::vector<uint8_t> Serialize() const;
        static bool Parse(const uint8_t* data, size_t size, Snapshot& out);
        std::wstring Describe() const;
    };

    enum class MsgType : uint8_t
    {
        Connect = 1, // snapshot, template name -> Ack
        Ack = 2,     // snapshot
        Delete = 3,  // snapshot, template name
        Ping = 4,    // -> Pong
        Pong = 5,    // u16 payload length, payload (published QoS private payload)
    };

    // Wire format: "PDNC" | type | nonce (4 bytes) | body
    struct Message
    {
        MsgType type{};
        uint32_t nonce = 0;
        std::vector<uint8_t> body;
        sockaddr_in6 from{};
        std::string peer; // EOS product user id when the message came over EOS P2P (then `from` is unused)
    };

    std::vector<uint8_t> Encode(MsgType type, uint32_t nonce, const std::vector<uint8_t>& body);
    bool Decode(const uint8_t* data, size_t size, Message& out);

    // This PC's own network address (primary IPv4), regardless of advertise_ip. In-process sockets reach each other on it.

    // Opens the control socket and starts the receive thread. Messages that aren't replies to a Request go to
    // `onUnsolicited`. Only the first call has any effect.
    void Start(std::function<void(const Message&)> onUnsolicited);

    // This PC's snapshot (random device id per run, address from config or the primary IPv4 route).
    const Snapshot& Local();

    void Send(const Snapshot& to, MsgType type, uint32_t nonce, const std::vector<uint8_t>& body);
    void SendTo(const sockaddr_in6& to, MsgType type, uint32_t nonce, const std::vector<uint8_t>& body);

    // Sends to `to`'s control port every `retryMs` until a reply with the same nonce arrives or `timeoutMs` passes.
    bool Request(const Snapshot& to, MsgType type, const std::vector<uint8_t>& body, uint32_t timeoutMs, uint32_t retryMs, Message& reply);

    uint32_t NewNonce();

    std::array<uint8_t, 16> InterfaceIp();

    sockaddr_in6 GameSockAddr(const std::array<uint8_t, 16>& ip, uint16_t port);
    std::wstring IpToString(const std::array<uint8_t, 16>& ip);

    std::wstring Base64Encode(const std::vector<uint8_t>& data);
    bool Base64Decode(std::wstring_view text, std::vector<uint8_t>& out);
}
