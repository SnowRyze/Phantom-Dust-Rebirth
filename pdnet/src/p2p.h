#pragma once
// Stage C: match traffic through EOS P2P (pdnet.ini: network_mode=eos, needs eos=on).
//
// EOS P2P connects two product users through home routers (NAT traversal) and falls back to Epic's relays when a
// direct path can't be made, so nobody forwards ports. The game is not changed: it still sends plain UDP from its own
// socket to whatever address the endpoint pair reports (sda.cpp). For a peer reached over EOS that address is a "link":
// a UDP socket inside the game process on this PC's own address. Datagrams the game sends to a link go out over EOS P2P
// to that peer; datagrams arriving from the peer are sent from the link to the game's socket, so the game sees one
// stable address per peer (FindEndpointPairBySocketAddressBytes maps it back).
//
// One EOS socket name ("PDNET"), three channels:
//   0 control: pdnet control messages (net::Message: connect/ack/delete), reliable and ordered
//   1 game:    the game's own datagrams, unreliable and unordered (like UDP)
//   2 ping:    ping/pong for QoS and link stats, unreliable (so losses show)
// Each link's ping, jitter, loss, direct-or-relayed path and traffic go to pdnet.log every 10 s ("p2p: link ...").
#include "net.h"
#include <memory>
#include <string>
#include <vector>

namespace pdnet::p2p
{
    // network_mode=eos and EOS signed in.
    bool Enabled();

    // Registers the EOS notifications and the packet pump (once). Control messages from peers that aren't replies
    // (connect, delete) go to `onControl` on a worker thread, with Message::peer set to the sender.
    void Start(std::function<void(const net::Message&)> onControl);

    void SendControl(const std::string& peer, net::MsgType type, uint32_t nonce, const std::vector<uint8_t>& body);
    // Sends a control message and waits for the reply with the same nonce (EOS opens the connection on the way).
    bool Request(const std::string& peer, net::MsgType type, const std::vector<uint8_t>& body, uint32_t timeoutMs, net::Message& reply);
    // Round trip in ms over the ping channel, or -1 on timeout. `payload` receives the peer's published QoS payload.
    int Ping(const std::string& peer, uint32_t timeoutMs, std::vector<uint8_t>* payload = nullptr);

    // The local UDP port the game sends to for `peer` (opens the link if needed); 0 on failure.
    uint16_t OpenLink(const std::string& peer);
    void CloseLink(const std::string& peer);
}
