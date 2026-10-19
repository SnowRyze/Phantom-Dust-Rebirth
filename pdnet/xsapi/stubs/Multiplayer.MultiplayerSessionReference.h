#pragma once
#include "Multiplayer/MultiplayerSessionReference.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionReference : MultiplayerSessionReferenceT<MultiplayerSessionReference>
    {
        MultiplayerSessionReference() = default;

        MultiplayerSessionReference(hstring const& serviceConfigurationId, hstring const& sessionTemplateName, hstring const& sessionName);
        static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference ParseFromUriPath(hstring const& uriPath);
        hstring ServiceConfigurationId();
        hstring SessionTemplateName();
        hstring SessionName();
        hstring ToUriPath();
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerSessionReference : MultiplayerSessionReferenceT<MultiplayerSessionReference, implementation::MultiplayerSessionReference>
    {
    };
}
