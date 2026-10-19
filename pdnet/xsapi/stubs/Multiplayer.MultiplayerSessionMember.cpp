#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionMember.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    uint32_t MultiplayerSessionMember::MemberId()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MemberId");
    }
    hstring MultiplayerSessionMember::TeamId()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::TeamId");
    }
    hstring MultiplayerSessionMember::XboxUserId()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::XboxUserId");
    }
    hstring MultiplayerSessionMember::MemberCustomConstantsJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MemberCustomConstantsJson");
    }
    hstring MultiplayerSessionMember::SecureDeviceAddressBase64()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::SecureDeviceAddressBase64");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> MultiplayerSessionMember::Roles()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Roles");
    }
    hstring MultiplayerSessionMember::MemberCustomPropertiesJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MemberCustomPropertiesJson");
    }
    hstring MultiplayerSessionMember::Gamertag()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Gamertag");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMemberStatus MultiplayerSessionMember::Status()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Status");
    }
    bool MultiplayerSessionMember::IsTurnAvailable()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::IsTurnAvailable");
    }
    bool MultiplayerSessionMember::IsCurrentUser()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::IsCurrentUser");
    }
    bool MultiplayerSessionMember::InitializeRequested()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::InitializeRequested");
    }
    hstring MultiplayerSessionMember::MatchmakingResultServerMeasurementsJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MatchmakingResultServerMeasurementsJson");
    }
    hstring MultiplayerSessionMember::MemberServerMeasurementsJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MemberServerMeasurementsJson");
    }
    Windows::Foundation::Collections::IVector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> MultiplayerSessionMember::MembersInGroup()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MembersInGroup");
    }
    Windows::Foundation::Collections::IVector<Microsoft::Xbox::Services::Multiplayer::MultiplayerQualityOfServiceMeasurements> MultiplayerSessionMember::MemberMeasurements()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::MemberMeasurements");
    }
    hstring MultiplayerSessionMember::DeviceToken()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::DeviceToken");
    }
    Microsoft::Xbox::Services::Multiplayer::NetworkAddressTranslationSetting MultiplayerSessionMember::Nat()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Nat");
    }
    uint32_t MultiplayerSessionMember::ActiveTitleId()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::ActiveTitleId");
    }
    uint32_t MultiplayerSessionMember::InitializationEpisode()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::InitializationEpisode");
    }
    Windows::Foundation::DateTime MultiplayerSessionMember::JoinTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::JoinTime");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerMeasurementFailure MultiplayerSessionMember::InitializationFailureCause()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::InitializationFailureCause");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSessionMember::TournamentTeamSessionRef()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::TournamentTeamSessionRef");
    }
    Windows::Foundation::Collections::IVector<hstring> MultiplayerSessionMember::Groups()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Groups");
    }
    void MultiplayerSessionMember::Groups(Windows::Foundation::Collections::IVector<hstring> const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Groups");
    }
    Windows::Foundation::Collections::IVector<hstring> MultiplayerSessionMember::Encounters()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Encounters");
    }
    void MultiplayerSessionMember::Encounters(Windows::Foundation::Collections::IVector<hstring> const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Encounters");
    }
    Windows::Foundation::Collections::IMap<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> MultiplayerSessionMember::Results()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::Results");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentArbitrationStatus MultiplayerSessionMember::ArbitrationStatus()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMember::ArbitrationStatus");
    }
}
