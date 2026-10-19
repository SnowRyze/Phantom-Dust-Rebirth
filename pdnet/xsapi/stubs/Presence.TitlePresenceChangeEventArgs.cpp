#include "xsapi_pch.h"
#include "Presence.TitlePresenceChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    hstring TitlePresenceChangeEventArgs::XboxUserId()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeEventArgs::XboxUserId");
    }
    uint32_t TitlePresenceChangeEventArgs::TitleId()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeEventArgs::TitleId");
    }
    Microsoft::Xbox::Services::Presence::TitlePresenceState TitlePresenceChangeEventArgs::TitleState()
    {
        PDNET_XSAPI_STUB("TitlePresenceChangeEventArgs::TitleState");
    }
}
