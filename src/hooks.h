#pragma once
#include <windows.h>

namespace pdnet
{
    // Points every loaded module's RoGetActivationFactory import (except ours) at our filter.
    // Safe to call repeatedly; already-patched modules are skipped.
    // `injected`: pdnet was loaded into an unmodified game by pdlaunch (see launcher/pdlaunch.cpp) instead of standing in for
    // Microsoft.Xbox.Services.dll, so with xbox=off the Xbox Live classes are answered here, and modules the game loads
    // later are hooked on the next activation.
    void InstallActivationHooks(HMODULE self, bool injected = false);

    // Injected mode: `handler` runs once, on the game's thread, before the first Microsoft.Xbox.Services.* class is
    // answered (pdnet's own set-up: log, settings, game patches, EOS).
    void SetFirstXboxActivationHandler(void (*handler)());
    bool FirstXboxActivationSeen();
}
