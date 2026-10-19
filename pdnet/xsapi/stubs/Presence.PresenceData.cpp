#include "xsapi_pch.h"
#include "Presence.PresenceData.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    PresenceData::PresenceData(hstring const& serviceConfigurationId, hstring const& presenceId)
    {
        PDNET_XSAPI_STUB("PresenceData::PresenceData");
    }
    PresenceData::PresenceData(hstring const& serviceConfigurationId, hstring const& presenceId, Windows::Foundation::Collections::IVectorView<hstring> const& presenceTokenIds)
    {
        PDNET_XSAPI_STUB("PresenceData::PresenceData");
    }
    hstring PresenceData::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("PresenceData::ServiceConfigurationId");
    }
    hstring PresenceData::PresenceId()
    {
        PDNET_XSAPI_STUB("PresenceData::PresenceId");
    }
    Windows::Foundation::Collections::IVectorView<hstring> PresenceData::PresenceTokenIds()
    {
        PDNET_XSAPI_STUB("PresenceData::PresenceTokenIds");
    }
}
