#pragma once
#include "Multiplayer/MultiplayerSessionProperties.g.h"
#include "../mpsd.h"

// pdnet-hand: live view of the session's properties (setters change the session being edited).
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionProperties : MultiplayerSessionPropertiesT<MultiplayerSessionProperties>
    {
        MultiplayerSessionProperties() : m_doc(std::make_shared<::pdnet::mpsd::SessionDoc>()) {}
        explicit MultiplayerSessionProperties(::pdnet::mpsd::SessionPtr doc) : m_doc(std::move(doc)) {}

        Windows::Foundation::Collections::IVectorView<hstring> Keywords()
        {
            std::vector<hstring> k;
            for (auto& w : m_doc->keywords) k.emplace_back(w);
            return winrt::single_threaded_vector(std::move(k)).GetView();
        }
        void Keywords(Windows::Foundation::Collections::IVectorView<hstring> const& value)
        {
            m_doc->keywords.clear();
            if (value) for (auto const& w : value) m_doc->keywords.emplace_back(w);
        }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction JoinRestriction() { return m_doc->joinRestriction; }
        void JoinRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction const& value) { m_doc->joinRestriction = value; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction ReadRestriction() { return m_doc->readRestriction; }
        void ReadRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction const& value) { m_doc->readRestriction = value; }
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> TurnCollection() { return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember>().GetView(); }
        void TurnCollection(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> const&) {}
        hstring MatchmakingTargetSessionConstantsJson() { return L"{}"; }
        hstring SessionCustomPropertiesJson() { return hstring{ m_doc->SessionPropertiesJson() }; }
        hstring MatchmakingServerConnectionString() { return {}; }
        Windows::Foundation::Collections::IVectorView<hstring> ServerConnectionStringCandidates() { return winrt::single_threaded_vector<hstring>().GetView(); }
        Windows::Foundation::Collections::IVectorView<uint32_t> SessionOwnerIndices() { return winrt::single_threaded_vector<uint32_t>().GetView(); }
        hstring HostDeviceToken() { return hstring{ m_doc->hostDeviceToken }; }
        bool Closed() { return m_doc->closed; }
        bool AllocateCloudCompute() { return false; }

    private:
        ::pdnet::mpsd::SessionPtr m_doc;
    };
}
