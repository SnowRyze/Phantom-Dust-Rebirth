#pragma once
#include "Multiplayer/Manager/MultiplayerLobbySession.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct MultiplayerLobbySession : MultiplayerLobbySessionT<MultiplayerLobbySession>
    {
        MultiplayerLobbySession() = default;

        hstring CorrelationId();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> LocalMembers();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> Members();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember Host();
        hstring Properties();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants SessionConstants();
        Microsoft::Xbox::Services::Tournaments::TournamentTeamResult LastTournamentTeamResult();
        void AddLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void RemoveLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void SetLocalMemberProperties(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context);
        void DeleteLocalMemberProperties(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, Windows::Foundation::IInspectable const& context);
        void SetLocalMemberConnectionAddress(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& connectionAddress, Windows::Foundation::IInspectable const& context);
        bool IsHost(hstring const& xboxUserId);
        void SetProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context);
        void SetSynchronizedProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context);
        void SetSynchronizedHost(Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember const& gameHost, Windows::Foundation::IInspectable const& context);
        void InviteFriends(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& contextStringId, hstring const& customActivationContext);
        void InviteUsers(Microsoft::Xbox::Services::System::XboxLiveUser const& user, Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds, hstring const& contextStringId, hstring const& customActivationContext);
    };
}
