#pragma once

namespace pdnet
{
    // eos_test=on: one-shot feasibility check that the Epic Online Services SDK works inside the game's sandbox.
    // On a background thread: load EOSSDK-Win64-Shipping.dll from the build folder, initialize, create the platform
    // (credentials from eos.ini next to the DLL), log in with a device ID (no Epic account), query the NAT type,
    // create and destroy a test lobby (presence off), then shut down. Results go to pdnet.log as "eos-test:" lines.
    // It never touches the game's own networking. Starts once.
    void StartEosTest();
}
