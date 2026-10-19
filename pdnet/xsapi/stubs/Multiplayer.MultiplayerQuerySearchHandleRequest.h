#pragma once
#include "Multiplayer/MultiplayerQuerySearchHandleRequest.g.h"

// pdnet-hand: plain data holder (the game fills it, GetSearchHandlesAsync reads it).
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerQuerySearchHandleRequest : MultiplayerQuerySearchHandleRequestT<MultiplayerQuerySearchHandleRequest>
    {
        MultiplayerQuerySearchHandleRequest() = default;

        MultiplayerQuerySearchHandleRequest(hstring const& serviceConfigurationId, hstring const& sessionTemplateName) : m_scid(serviceConfigurationId), m_template(sessionTemplateName) {}
        hstring ServiceConfigurationId() { return m_scid; }
        hstring SessionTemplateName() { return m_template; }
        hstring OrderBy() { return m_orderBy; }
        void OrderBy(hstring const& value) { m_orderBy = value; }
        bool OrderAscending() { return m_ascending; }
        void OrderAscending(bool value) { m_ascending = value; }
        hstring SearchFilter() { return m_filter; }
        void SearchFilter(hstring const& value) { m_filter = value; }
        hstring SocialGroup() { return m_socialGroup; }
        void SocialGroup(hstring const& value) { m_socialGroup = value; }

    private:
        hstring m_scid, m_template, m_orderBy, m_filter, m_socialGroup;
        bool m_ascending = false;
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerQuerySearchHandleRequest : MultiplayerQuerySearchHandleRequestT<MultiplayerQuerySearchHandleRequest, implementation::MultiplayerQuerySearchHandleRequest>
    {
    };
}
