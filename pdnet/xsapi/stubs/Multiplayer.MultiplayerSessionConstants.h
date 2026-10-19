#pragma once
#include "Multiplayer/MultiplayerSessionConstants.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionConstants : MultiplayerSessionConstantsT<MultiplayerSessionConstants>
    {
        MultiplayerSessionConstants() = default;

        uint32_t MaxMembersInSession();
        void MaxMembersInSession(uint32_t value);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility MultiplayerSessionVisibility();
        void MultiplayerSessionVisibility(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& value);
        Windows::Foundation::Collections::IVectorView<hstring> InitiatorXboxUserIds();
        hstring CustomConstantsJson();
        Windows::Foundation::TimeSpan MemberReservationTimeout();
        Windows::Foundation::TimeSpan MemberInactiveTimeout();
        Windows::Foundation::TimeSpan MemberReadyTimeout();
        Windows::Foundation::TimeSpan SessionEmptyTimeout();
        Windows::Foundation::TimeSpan ArbitrationTimeout();
        Windows::Foundation::TimeSpan ForfeitTimeout();
        bool CapabilitiesConnectivity();
        bool CapabilitiesSuppressPresenceActivityCheck();
        bool CapabilitiesGameplay();
        bool CapabilitiesLarge();
        bool CapabilitiesConnectionRequiredForActiveMember();
        bool CapabilitiesCrossplay();
        bool CapabilitiesUserAuthorizationStyle();
        bool CapabilitiesTeam();
        bool CapabilitiesSearchable();
        bool CapabilitiesArbitration();
        bool EnableMetricsLatency();
        bool EnableMetricsBandwidthDown();
        bool EnableMetricsBandwidthUp();
        bool EnableMetricsCustom();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerManagedInitialization ManagedInitialization();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerMemberInitialization MemberInitialization();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerPeerToPeerRequirements PeerToPeerRequirements();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerPeerToHostRequirements PeerToHostRequirements();
        hstring MeasurementServerAddressesJson();
    };
}
