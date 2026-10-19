#include "xsapi_pch.h"
#include "Multiplayer.Manager.MultiplayerGameSession.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    hstring MultiplayerGameSession::CorrelationId()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::CorrelationId");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerGameSession::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::SessionReference");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> MultiplayerGameSession::Members()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::Members");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember MultiplayerGameSession::Host()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::Host");
    }
    hstring MultiplayerGameSession::Properties()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::Properties");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionConstants MultiplayerGameSession::SessionConstants()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::SessionConstants");
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference> MultiplayerGameSession::TournamentTeams()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::TournamentTeams");
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> MultiplayerGameSession::TournamentTeamResults()
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::TournamentTeamResults");
    }
    bool MultiplayerGameSession::IsHost(hstring const& xboxUserId)
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::IsHost");
    }
    void MultiplayerGameSession::SetProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::SetProperties");
    }
    void MultiplayerGameSession::SetSynchronizedProperties(hstring const& name, hstring const& valueJson, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::SetSynchronizedProperties");
    }
    void MultiplayerGameSession::SetSynchronizedHost(Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember const& gameHost, Windows::Foundation::IInspectable const& context)
    {
        PDNET_XSAPI_STUB("MultiplayerGameSession::SetSynchronizedHost");
    }
}
