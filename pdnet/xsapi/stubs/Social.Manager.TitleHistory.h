#pragma once
#include "Social/Manager/TitleHistory.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct TitleHistory : TitleHistoryT<TitleHistory>
    {
        TitleHistory() = default;

        bool HasUserPlayed();
        Windows::Foundation::DateTime LastTimeUserPlayed();
    };
}
