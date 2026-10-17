#pragma once
#include <windows.h>

namespace pdnet
{
    // Points every loaded module's RoGetActivationFactory import (except ours) at our filter.
    // Safe to call repeatedly; already-patched modules are skipped.
    void InstallActivationHooks(HMODULE self);
}
