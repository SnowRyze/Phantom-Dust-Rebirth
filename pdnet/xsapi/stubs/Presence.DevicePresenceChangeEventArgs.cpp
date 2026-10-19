#include "xsapi_pch.h"
#include "Presence.DevicePresenceChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    hstring DevicePresenceChangeEventArgs::XboxUserId()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeEventArgs::XboxUserId");
    }
    Microsoft::Xbox::Services::Presence::PresenceDeviceType DevicePresenceChangeEventArgs::DeviceType()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeEventArgs::DeviceType");
    }
    bool DevicePresenceChangeEventArgs::IsUserLoggedOnDevice()
    {
        PDNET_XSAPI_STUB("DevicePresenceChangeEventArgs::IsUserLoggedOnDevice");
    }
}
