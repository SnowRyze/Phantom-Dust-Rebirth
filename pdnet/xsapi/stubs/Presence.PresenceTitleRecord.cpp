#include "xsapi_pch.h"
#include "Presence.PresenceTitleRecord.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    uint32_t PresenceTitleRecord::TitleId()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::TitleId");
    }
    hstring PresenceTitleRecord::TitleName()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::TitleName");
    }
    Windows::Foundation::DateTime PresenceTitleRecord::LastModifiedDate()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::LastModifiedDate");
    }
    bool PresenceTitleRecord::IsTitleActive()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::IsTitleActive");
    }
    hstring PresenceTitleRecord::Presence()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::Presence");
    }
    Microsoft::Xbox::Services::Presence::PresenceTitleViewState PresenceTitleRecord::TitleViewState()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::TitleViewState");
    }
    Microsoft::Xbox::Services::Presence::PresenceBroadcastRecord PresenceTitleRecord::BroadcastRecord()
    {
        PDNET_XSAPI_STUB("PresenceTitleRecord::BroadcastRecord");
    }
}
