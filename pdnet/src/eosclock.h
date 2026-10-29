#pragma once
#include <windows.h>

namespace pdnet
{
    // Two PCs whose clocks are more than about a day apart can't play together: EOS connects them, then nothing passes
    // (seen 2026-10-29 with a PC 31 h fast; with its clock set right the join worked at once). pdlaunch measures how far
    // this PC's clock is from Epic's before the game starts and leaves the number of seconds to add in
    // LocalState\pdnet\clock-offset.txt. This makes the EOS SDK module, and only it, see the corrected time: its
    // wall-clock imports (GetSystemTimeAsFileTime, GetSystemTime, GetLocalTime, _time64, _ftime64) are pointed at
    // versions that add the offset. The Windows clock itself is left alone (changing it needs administrator rights).
    // Returns the offset applied in seconds (0: none needed or none known).
    long long CorrectEosClock(HMODULE eosModule);
}
