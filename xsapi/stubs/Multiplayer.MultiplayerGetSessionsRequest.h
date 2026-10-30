#pragma once
#include "Multiplayer/MultiplayerGetSessionsRequest.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerGetSessionsRequest : MultiplayerGetSessionsRequestT<MultiplayerGetSessionsRequest>
    {
        MultiplayerGetSessionsRequest() = default;

        MultiplayerGetSessionsRequest(hstring const& serviceConfigurationId, uint32_t maxItems);
        hstring ServiceConfigurationId();
        uint32_t MaxItems();
        bool IncludePrivateSessions();
        void IncludePrivateSessions(bool value);
        bool IncludeReservations();
        void IncludeReservations(bool value);
        bool IncludeInactiveSessions();
        void IncludeInactiveSessions(bool value);
        hstring XboxUserIdFilter();
        void XboxUserIdFilter(hstring const& value);
        Windows::Foundation::Collections::IVectorView<hstring> XboxUserIdsFilter();
        void XboxUserIdsFilter(Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds);
        hstring KeywordFilter();
        void KeywordFilter(hstring const& value);
        hstring SessionTemplateNameFilter();
        void SessionTemplateNameFilter(hstring const& value);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility VisibilityFilter();
        void VisibilityFilter(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& value);
        uint32_t ContractVersionFilter();
        void ContractVersionFilter(uint32_t value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerGetSessionsRequest : MultiplayerGetSessionsRequestT<MultiplayerGetSessionsRequest, implementation::MultiplayerGetSessionsRequest>
    {
    };
}
