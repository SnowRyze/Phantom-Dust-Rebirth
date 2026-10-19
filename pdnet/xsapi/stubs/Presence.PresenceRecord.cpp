#include "xsapi_pch.h"
#include "Presence.PresenceRecord.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    hstring PresenceRecord::XboxUserId()
    {
        PDNET_XSAPI_STUB("PresenceRecord::XboxUserId");
    }
    Microsoft::Xbox::Services::Presence::UserPresenceState PresenceRecord::UserState()
    {
        PDNET_XSAPI_STUB("PresenceRecord::UserState");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceDeviceRecord> PresenceRecord::PresenceDeviceRecords()
    {
        PDNET_XSAPI_STUB("PresenceRecord::PresenceDeviceRecords");
    }
    bool PresenceRecord::IsUserPlayingTitle(uint32_t titleId)
    {
        PDNET_XSAPI_STUB("PresenceRecord::IsUserPlayingTitle");
    }
}
