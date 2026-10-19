#pragma once
#include "Social/Manager/SocialManagerPresenceTitleRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct SocialManagerPresenceTitleRecord : SocialManagerPresenceTitleRecordT<SocialManagerPresenceTitleRecord>
    {
        SocialManagerPresenceTitleRecord() = default;

        bool IsTitleActive();
        uint32_t TitleId();
        hstring PresenceText();
        bool IsBroadcasting();
        Microsoft::Xbox::Services::Presence::PresenceDeviceType DeviceType();
    };
}
