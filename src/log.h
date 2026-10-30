#pragma once
#include <string>

namespace pdnet
{
    // Appends a line to LocalState\pdnet.log and echoes it to OutputDebugString.
    void Log(const wchar_t* fmt, ...);

    // LocalState folder of the running package (empty if unavailable).
    const std::wstring& LocalStatePath();
}
