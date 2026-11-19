#pragma once
#include <string>

namespace pdnet
{
    // Appends a line to LocalState\pdnet.log and echoes it to OutputDebugString.
    void Log(const wchar_t* fmt, ...);

    // The in-game name the log shows in place of the PC's own names (logscrub.h). Read from settings.ini / pdnet.ini on
    // the first line; set again when the config loads or the name is changed in Online Settings.
    void SetLogName(const std::wstring& name);

    // LocalState folder of the running package (empty if unavailable).
    const std::wstring& LocalStatePath();
}
