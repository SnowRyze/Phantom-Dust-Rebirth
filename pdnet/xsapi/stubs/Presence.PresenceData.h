#pragma once
#include "Presence/PresenceData.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceData : PresenceDataT<PresenceData>
    {
        PresenceData() = default;

        PresenceData(hstring const& serviceConfigurationId, hstring const& presenceId);
        PresenceData(hstring const& serviceConfigurationId, hstring const& presenceId, Windows::Foundation::Collections::IVectorView<hstring> const& presenceTokenIds);
        hstring ServiceConfigurationId();
        hstring PresenceId();
        Windows::Foundation::Collections::IVectorView<hstring> PresenceTokenIds();
    };
}
namespace winrt::Microsoft::Xbox::Services::Presence::factory_implementation
{
    struct PresenceData : PresenceDataT<PresenceData, implementation::PresenceData>
    {
    };
}
