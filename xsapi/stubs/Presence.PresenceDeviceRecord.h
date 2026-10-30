#pragma once
#include "Presence/PresenceDeviceRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceDeviceRecord : PresenceDeviceRecordT<PresenceDeviceRecord>
    {
        PresenceDeviceRecord() = default;

        Microsoft::Xbox::Services::Presence::PresenceDeviceType DeviceType();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceTitleRecord> PresenceTitleRecords();
    };
}
