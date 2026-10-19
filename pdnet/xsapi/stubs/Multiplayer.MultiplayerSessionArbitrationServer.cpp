#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionArbitrationServer.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::DateTime MultiplayerSessionArbitrationServer::ArbitrationStartTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionArbitrationServer::ArbitrationStartTime");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentArbitrationState MultiplayerSessionArbitrationServer::ResultState()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionArbitrationServer::ResultState");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentGameResultSource MultiplayerSessionArbitrationServer::ResultSource()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionArbitrationServer::ResultSource");
    }
    uint32_t MultiplayerSessionArbitrationServer::ResultConfidenceLevel()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionArbitrationServer::ResultConfidenceLevel");
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Tournaments::TournamentTeamResult> MultiplayerSessionArbitrationServer::Results()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionArbitrationServer::Results");
    }
}
