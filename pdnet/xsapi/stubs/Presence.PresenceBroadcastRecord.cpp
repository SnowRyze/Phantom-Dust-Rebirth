#include "xsapi_pch.h"
#include "Presence.PresenceBroadcastRecord.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    hstring PresenceBroadcastRecord::BroadcastId()
    {
        PDNET_XSAPI_STUB("PresenceBroadcastRecord::BroadcastId");
    }
    hstring PresenceBroadcastRecord::Session()
    {
        PDNET_XSAPI_STUB("PresenceBroadcastRecord::Session");
    }
    hstring PresenceBroadcastRecord::Provider()
    {
        PDNET_XSAPI_STUB("PresenceBroadcastRecord::Provider");
    }
    uint32_t PresenceBroadcastRecord::ViewerCount()
    {
        PDNET_XSAPI_STUB("PresenceBroadcastRecord::ViewerCount");
    }
    Windows::Foundation::DateTime PresenceBroadcastRecord::StartTime()
    {
        PDNET_XSAPI_STUB("PresenceBroadcastRecord::StartTime");
    }
}
