#pragma once
#include "Presence/PresenceRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceRecord : PresenceRecordT<PresenceRecord>
    {
        PresenceRecord() = default;

        hstring XboxUserId();
        Microsoft::Xbox::Services::Presence::UserPresenceState UserState();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceDeviceRecord> PresenceDeviceRecords();
        bool IsUserPlayingTitle(uint32_t titleId);
    };
}
