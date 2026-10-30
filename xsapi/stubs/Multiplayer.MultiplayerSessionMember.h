#pragma once
#include "Multiplayer/MultiplayerSessionMember.g.h"
#include "../mpsd.h"

// pdnet-hand: a member as stored in the session (a snapshot).
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionMember : MultiplayerSessionMemberT<MultiplayerSessionMember>
    {
        MultiplayerSessionMember() = default;
        explicit MultiplayerSessionMember(::pdnet::mpsd::MemberDoc doc) : m_doc(std::move(doc)) {}

        uint32_t MemberId() { return m_doc.memberId; }
        hstring TeamId() { return {}; }
        hstring XboxUserId() { return hstring{ m_doc.xuid }; }
        hstring MemberCustomConstantsJson() { return hstring{ m_doc.customConstantsJson.empty() ? L"{}" : m_doc.customConstantsJson }; }
        hstring SecureDeviceAddressBase64() { return hstring{ m_doc.deviceAddressBase64 }; }
        Windows::Foundation::Collections::IMapView<hstring, hstring> Roles() { return winrt::single_threaded_map<hstring, hstring>().GetView(); }
        hstring MemberCustomPropertiesJson() { return hstring{ ::pdnet::mpsd::JsonObject(m_doc.customProperties) }; }
        hstring Gamertag() { return hstring{ m_doc.gamertag }; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus Status() { return m_doc.status; }
        bool IsTurnAvailable() { return false; }
        bool IsCurrentUser() { return m_doc.xuid == ::pdnet::xsapi::LocalXuid(); }
        bool InitializeRequested() { return m_doc.initializeRequested; }
        hstring MatchmakingResultServerMeasurementsJson() { return {}; }
        hstring MemberServerMeasurementsJson() { return {}; }
        Windows::Foundation::Collections::IVector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> MembersInGroup() { return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember>(); }
        Windows::Foundation::Collections::IVector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> MemberMeasurements() { return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements>(); }
        hstring DeviceToken() { return hstring{ m_doc.xuid }; } // one device per player, so the player id doubles as the device token
        Microsoft::Xbox::Services::Multiplayer::NetworkAddressTranslationSetting Nat() { return Microsoft::Xbox::Services::Multiplayer::NetworkAddressTranslationSetting::Open; }
        uint32_t ActiveTitleId() { return 267248679; }
        uint32_t InitializationEpisode() { return 0; }
        Windows::Foundation::DateTime JoinTime() { return m_doc.joinTime; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerMeasurementFailure InitializationFailureCause() { return {}; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference TournamentTeamSessionRef() { return nullptr; }
        Windows::Foundation::Collections::IVector<hstring> Groups() { return m_groups; }
        void Groups(Windows::Foundation::Collections::IVector<hstring> const& value) { m_groups = value; }
        Windows::Foundation::Collections::IVector<hstring> Encounters() { return m_encounters; }
        void Encounters(Windows::Foundation::Collections::IVector<hstring> const& value) { m_encounters = value; }
        Windows::Foundation::Collections::IMap<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> Results() { return winrt::single_threaded_map<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult>(); }
        Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus ArbitrationStatus() { return {}; }

        const ::pdnet::mpsd::MemberDoc& Doc() const { return m_doc; }

    private:
        ::pdnet::mpsd::MemberDoc m_doc;
        Windows::Foundation::Collections::IVector<hstring> m_groups = winrt::single_threaded_vector<hstring>();
        Windows::Foundation::Collections::IVector<hstring> m_encounters = winrt::single_threaded_vector<hstring>();
    };
}
