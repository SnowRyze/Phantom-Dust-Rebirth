#pragma once
#include "Multiplayer/MultiplayerQuerySearchHandleRequest.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerQuerySearchHandleRequest : MultiplayerQuerySearchHandleRequestT<MultiplayerQuerySearchHandleRequest>
    {
        MultiplayerQuerySearchHandleRequest() = default;

        MultiplayerQuerySearchHandleRequest(hstring const& serviceConfigurationId, hstring const& sessionTemplateName);
        hstring ServiceConfigurationId();
        hstring SessionTemplateName();
        hstring OrderBy();
        void OrderBy(hstring const& value);
        bool OrderAscending();
        void OrderAscending(bool value);
        hstring SearchFilter();
        void SearchFilter(hstring const& value);
        hstring SocialGroup();
        void SocialGroup(hstring const& value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerQuerySearchHandleRequest : MultiplayerQuerySearchHandleRequestT<MultiplayerQuerySearchHandleRequest, implementation::MultiplayerQuerySearchHandleRequest>
    {
    };
}
