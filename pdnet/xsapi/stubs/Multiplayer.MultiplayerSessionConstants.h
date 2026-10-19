#pragma once
#include "Multiplayer/MultiplayerSessionConstants.g.h"
#include "../mpsd.h"

// pdnet-hand: view of the session's constants. Capabilities match a peer-to-host game session with connectivity checks off.
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionConstants : MultiplayerSessionConstantsT<MultiplayerSessionConstants>
    {
        MultiplayerSessionConstants() : m_doc(std::make_shared<::pdnet::mpsd::SessionDoc>()) {}
        explicit MultiplayerSessionConstants(::pdnet::mpsd::SessionPtr doc) : m_doc(std::move(doc)) {}

        uint32_t MaxMembersInSession() { return m_doc->maxMembers; }
        void MaxMembersInSession(uint32_t value) { m_doc->maxMembers = value; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility MultiplayerSessionVisibility() { return m_doc->visibility; }
        void MultiplayerSessionVisibility(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& value) { m_doc->visibility = value; }
        Windows::Foundation::Collections::IVectorView<hstring> InitiatorXboxUserIds()
        {
            std::vector<hstring> ids;
            for (auto& x : m_doc->initiators) ids.emplace_back(x);
            return winrt::single_threaded_vector(std::move(ids)).GetView();
        }
        hstring CustomConstantsJson() { return hstring{ m_doc->customConstantsJson.empty() ? L"{}" : m_doc->customConstantsJson }; }
        Windows::Foundation::TimeSpan MemberReservationTimeout() { return std::chrono::seconds(30); }
        Windows::Foundation::TimeSpan MemberInactiveTimeout() { return std::chrono::seconds(30); }
        Windows::Foundation::TimeSpan MemberReadyTimeout() { return std::chrono::seconds(180); }
        Windows::Foundation::TimeSpan SessionEmptyTimeout() { return std::chrono::seconds(0); }
        Windows::Foundation::TimeSpan ArbitrationTimeout() { return {}; }
        Windows::Foundation::TimeSpan ForfeitTimeout() { return {}; }
        bool CapabilitiesConnectivity() { return false; }
        bool CapabilitiesSuppressPresenceActivityCheck() { return true; }
        bool CapabilitiesGameplay() { return true; }
        bool CapabilitiesLarge() { return false; }
        bool CapabilitiesConnectionRequiredForActiveMember() { return false; }
        bool CapabilitiesCrossplay() { return false; }
        bool CapabilitiesUserAuthorizationStyle() { return false; }
        bool CapabilitiesTeam() { return false; }
        bool CapabilitiesSearchable() { return true; }
        bool CapabilitiesArbitration() { return false; }
        bool EnableMetricsLatency() { return false; }
        bool EnableMetricsBandwidthDown() { return false; }
        bool EnableMetricsBandwidthUp() { return false; }
        bool EnableMetricsCustom() { return false; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerManagedInitialization ManagedInitialization();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerMemberInitialization MemberInitialization();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerPeerToPeerRequirements PeerToPeerRequirements();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerPeerToHostRequirements PeerToHostRequirements();
        hstring MeasurementServerAddressesJson() { return L"[]"; }

    private:
        ::pdnet::mpsd::SessionPtr m_doc;
    };
}
