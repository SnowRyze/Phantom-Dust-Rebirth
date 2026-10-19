#include "xsapi_pch.h"
#include "Statistics.Manager.StatisticEvent.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    Microsoft::Xbox::Services::System::XboxLiveUser StatisticEvent::User()
    {
        PDNET_XSAPI_STUB("StatisticEvent::User");
    }
    Microsoft::Xbox::Services::Statistics::Manager::StatisticEventType StatisticEvent::EventType()
    {
        PDNET_XSAPI_STUB("StatisticEvent::EventType");
    }
    int32_t StatisticEvent::ErrorCode()
    {
        PDNET_XSAPI_STUB("StatisticEvent::ErrorCode");
    }
    hstring StatisticEvent::ErrorMessage()
    {
        PDNET_XSAPI_STUB("StatisticEvent::ErrorMessage");
    }
    Microsoft::Xbox::Services::Statistics::Manager::StatisticEventArgs StatisticEvent::EventArgs()
    {
        PDNET_XSAPI_STUB("StatisticEvent::EventArgs");
    }
}
