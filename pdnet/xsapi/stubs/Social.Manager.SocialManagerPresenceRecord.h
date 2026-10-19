#pragma once
#include "Social/Manager/SocialManagerPresenceRecord.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct SocialManagerPresenceRecord : SocialManagerPresenceRecordT<SocialManagerPresenceRecord>
    {
        SocialManagerPresenceRecord() = default;

        Microsoft::Xbox::Services::Presence::UserPresenceState UserState();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::SocialManagerPresenceTitleRecord> PresenceTitleRecords();
        bool IsUserPlayingTitle(uint32_t titleId);
    };
}
