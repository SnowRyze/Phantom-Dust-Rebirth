#include "xsapi_pch.h"
#include "Presence.PresenceMediaRecord.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    hstring PresenceMediaRecord::MediaId()
    {
        PDNET_XSAPI_STUB("PresenceMediaRecord::MediaId");
    }
    Microsoft::Xbox::Services::Presence::PresenceMediaIdType PresenceMediaRecord::MediaIdType()
    {
        PDNET_XSAPI_STUB("PresenceMediaRecord::MediaIdType");
    }
    hstring PresenceMediaRecord::Name()
    {
        PDNET_XSAPI_STUB("PresenceMediaRecord::Name");
    }
}
