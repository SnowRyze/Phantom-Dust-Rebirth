#pragma once
#include "Presence/PresenceBroadcastRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceBroadcastRecord : PresenceBroadcastRecordT<PresenceBroadcastRecord>
    {
        PresenceBroadcastRecord() = default;

        hstring BroadcastId();
        hstring Session();
        hstring Provider();
        uint32_t ViewerCount();
        Windows::Foundation::DateTime StartTime();
    };
}
