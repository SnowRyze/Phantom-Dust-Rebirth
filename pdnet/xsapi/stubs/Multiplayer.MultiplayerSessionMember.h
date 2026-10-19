#pragma once
#include "Multiplayer/MultiplayerSessionMember.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionMember : MultiplayerSessionMemberT<MultiplayerSessionMember>
    {
        MultiplayerSessionMember() = default;

        uint32_t MemberId();
        hstring TeamId();
        hstring XboxUserId();
        hstring MemberCustomConstantsJson();
        hstring SecureDeviceAddressBase64();
        Windows::Foundation::Collections::IMapView<hstring, hstring> Roles();
        hstring MemberCustomPropertiesJson();
        hstring Gamertag();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus Status();
        bool IsTurnAvailable();
        bool IsCurrentUser();
        bool InitializeRequested();
        hstring MatchmakingResultServerMeasurementsJson();
        hstring MemberServerMeasurementsJson();
        Windows::Foundation::Collections::IVector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> MembersInGroup();
        Windows::Foundation::Collections::IVector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> MemberMeasurements();
        hstring DeviceToken();
        Microsoft::Xbox::Services::Multiplayer::NetworkAddressTranslationSetting Nat();
        uint32_t ActiveTitleId();
        uint32_t InitializationEpisode();
        Windows::Foundation::DateTime JoinTime();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerMeasurementFailure InitializationFailureCause();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference TournamentTeamSessionRef();
        Windows::Foundation::Collections::IVector<hstring> Groups();
        void Groups(Windows::Foundation::Collections::IVector<hstring> const& value);
        Windows::Foundation::Collections::IVector<hstring> Encounters();
        void Encounters(Windows::Foundation::Collections::IVector<hstring> const& value);
        Windows::Foundation::Collections::IMap<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> Results();
        Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus ArbitrationStatus();
    };
}
