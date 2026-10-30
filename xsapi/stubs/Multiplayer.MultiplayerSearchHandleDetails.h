#pragma once
#include "Multiplayer/MultiplayerSearchHandleDetails.g.h"
#include "../mpsd.h"

// pdnet-hand: one lobby browser entry, a snapshot of a session that has a search handle.
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSearchHandleDetails : MultiplayerSearchHandleDetailsT<MultiplayerSearchHandleDetails>
    {
        MultiplayerSearchHandleDetails() : m_doc(std::make_shared<::pdnet::mpsd::SessionDoc>()) { m_doc->search.emplace(); }
        explicit MultiplayerSearchHandleDetails(::pdnet::mpsd::SessionPtr doc) : m_doc(std::move(doc)) { if (!m_doc->search) m_doc->search.emplace(); }

        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        hstring HandleId() { return hstring{ m_doc->search->id }; }
        Windows::Foundation::Collections::IVectorView<hstring> SessionOwnerXboxUserIds();
        Windows::Foundation::Collections::IVectorView<hstring> Tags();
        Windows::Foundation::Collections::IMapView<hstring, double> NumbersMetadata();
        Windows::Foundation::Collections::IMapView<hstring, hstring> StringsMetadata();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> RoleTypes();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility Visibility() { return m_doc->visibility; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction JoinRestriction() { return m_doc->joinRestriction; }
        bool Closed() { return m_doc->closed; }
        uint32_t MaxMembersCount() { return m_doc->maxMembers; }
        uint32_t MembersCount();
        Windows::Foundation::DateTime HandleCreationTime() { return m_doc->search->created; }
        hstring CustomSessionPropertiesJson() { return hstring{ m_doc->SessionPropertiesJson() }; }

        const ::pdnet::mpsd::SessionDoc& Doc() const { return *m_doc; }

    private:
        ::pdnet::mpsd::SessionPtr m_doc;
    };
}
