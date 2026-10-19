#include "xsapi_pch.h"
#include "Multiplayer.Manager.MultiplayerLobbySession.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    hstring MultiplayerLobbySession::CorrelationId()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::CorrelationId");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerLobbySession::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SessionReference");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> MultiplayerLobbySession::LocalMembers()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::LocalMembers");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> MultiplayerLobbySession::Members()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::Members");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember MultiplayerLobbySession::Host()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::Host");
    }
    hstring MultiplayerLobbySession::Properties()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::Properties");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants MultiplayerLobbySession::SessionConstants()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SessionConstants");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentTeamResult MultiplayerLobbySession::LastTournamentTeamResult()
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::LastTournamentTeamResult");
    }
    void MultiplayerLobbySession::AddLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::AddLocalUser");
    }
    void MultiplayerLobbySession::RemoveLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::RemoveLocalUser");
    }
    void MultiplayerLobbySession::SetLocalMemberProperties(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SetLocalMemberProperties");
    }
    void MultiplayerLobbySession::DeleteLocalMemberProperties(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::DeleteLocalMemberProperties");
    }
    void MultiplayerLobbySession::SetLocalMemberConnectionAddress(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& connectionAddress, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SetLocalMemberConnectionAddress");
    }
    bool MultiplayerLobbySession::IsHost(hstring const& xboxUserId)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::IsHost");
    }
    void MultiplayerLobbySession::SetProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SetProperties");
    }
    void MultiplayerLobbySession::SetSynchronizedProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SetSynchronizedProperties");
    }
    void MultiplayerLobbySession::SetSynchronizedHost(Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember const& gameHost, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::SetSynchronizedHost");
    }
    void MultiplayerLobbySession::InviteFriends(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& contextStringId, hstring const& customActivationContext)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::InviteFriends");
    }
    void MultiplayerLobbySession::InviteUsers(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds, hstring const& contextStringId, hstring const& customActivationContext)
    {
        PDNET_XSAPI_STUB("MultiplayerLobbySession::InviteUsers");
    }
}
