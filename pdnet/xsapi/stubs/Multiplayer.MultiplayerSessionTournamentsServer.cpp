#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionTournamentsServer.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Tournaments::TournamentReference MultiplayerSessionTournamentsServer::TournamentReference()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::TournamentReference");
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference> MultiplayerSessionTournamentsServer::Teams()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::Teams");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentRegistrationState MultiplayerSessionTournamentsServer::RegistrationState()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::RegistrationState");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentRegistrationReason MultiplayerSessionTournamentsServer::RegistrationReason()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::RegistrationReason");
    }
    Windows::Foundation::DateTime MultiplayerSessionTournamentsServer::NextGameStartTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::NextGameStartTime");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSessionTournamentsServer::NextGameSessionRef()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::NextGameSessionRef");
    }
    Windows::Foundation::DateTime MultiplayerSessionTournamentsServer::LastGameEndTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::LastGameEndTime");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentTeamResult MultiplayerSessionTournamentsServer::LastTeamResult()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::LastTeamResult");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentGameResultSource MultiplayerSessionTournamentsServer::LastGameResultSource()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionTournamentsServer::LastGameResultSource");
    }
}
