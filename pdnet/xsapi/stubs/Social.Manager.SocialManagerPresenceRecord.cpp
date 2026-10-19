#include "xsapi_pch.h"
#include "Social.Manager.SocialManagerPresenceRecord.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    Microsoft::Xbox::Services::Presence::UserPresenceState SocialManagerPresenceRecord::UserState()
    {
        PDNET_XSAPI_STUB("SocialManagerPresenceRecord::UserState");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Social::Manager::SocialManagerPresenceTitleRecord> SocialManagerPresenceRecord::PresenceTitleRecords()
    {
        PDNET_XSAPI_STUB("SocialManagerPresenceRecord::PresenceTitleRecords");
    }
    bool SocialManagerPresenceRecord::IsUserPlayingTitle(uint32_t titleId)
    {
        PDNET_XSAPI_STUB("SocialManagerPresenceRecord::IsUserPlayingTitle");
    }
}
