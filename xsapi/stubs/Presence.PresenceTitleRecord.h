#pragma once
#include "Presence/PresenceTitleRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceTitleRecord : PresenceTitleRecordT<PresenceTitleRecord>
    {
        PresenceTitleRecord() = default;

        uint32_t TitleId();
        hstring TitleName();
        Windows::Foundation::DateTime LastModifiedDate();
        bool IsTitleActive();
        hstring Presence();
        Microsoft::Xbox::Services::Presence::PresenceTitleViewState TitleViewState();
        Microsoft::Xbox::Services::Presence::PresenceBroadcastRecord BroadcastRecord();
    };
}
