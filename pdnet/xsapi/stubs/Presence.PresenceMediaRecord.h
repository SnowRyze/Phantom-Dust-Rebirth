#pragma once
#include "Presence/PresenceMediaRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceMediaRecord : PresenceMediaRecordT<PresenceMediaRecord>
    {
        PresenceMediaRecord() = default;

        hstring MediaId();
        Microsoft::Xbox::Services::Presence::PresenceMediaIdType MediaIdType();
        hstring Name();
    };
}
