#pragma once
#include "Multiplayer/Manager/MultiplayerGameSession.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct MultiplayerGameSession : MultiplayerGameSessionT<MultiplayerGameSession>
    {
        MultiplayerGameSession() = default;

        hstring CorrelationId();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> Members();
        Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember Host();
        hstring Properties();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants SessionConstants();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference> TournamentTeams();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> TournamentTeamResults();
        bool IsHost(hstring const& xboxUserId);
        void SetProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context);
        void SetSynchronizedProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context);
        void SetSynchronizedHost(Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember const& gameHost, Windows::Foundation::IInspectable const& context);
    };
}
