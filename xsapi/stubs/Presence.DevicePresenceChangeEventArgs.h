#pragma once
#include "Presence/DevicePresenceChangeEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct DevicePresenceChangeEventArgs : DevicePresenceChangeEventArgsT<DevicePresenceChangeEventArgs>
    {
        DevicePresenceChangeEventArgs() = default;

        hstring XboxUserId();
        Microsoft::Xbox::Services::Presence::PresenceDeviceType DeviceType();
        bool IsUserLoggedOnDevice();
    };
}
