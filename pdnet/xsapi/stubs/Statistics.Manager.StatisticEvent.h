#pragma once
#include "Statistics/Manager/StatisticEvent.g.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    struct StatisticEvent : StatisticEventT<StatisticEvent>
    {
        StatisticEvent() = default;

        Microsoft::Xbox::Services::System::XboxLiveUser User();
        Microsoft::Xbox::Services::Statistics::Manager::StatisticEventType EventType();
        int32_t ErrorCode();
        hstring ErrorMessage();
        Microsoft::Xbox::Services::Statistics::Manager::StatisticEventArgs EventArgs();
    };
}
