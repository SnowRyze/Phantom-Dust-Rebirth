#pragma once
#include <string>

namespace pdnet
{
    // Debug aid (pdnet.ini: test_peer=on). Pretends to be a second PC joining this one, from inside the same
    // process: connects through the pdnet control channel, says HELLO on the game port and logs what the game
    // sends back. Starts a few seconds after the game first asks for an endpoint pair template (lobby creation).
    void StartTestPeer(const std::wstring& templateName);
}
