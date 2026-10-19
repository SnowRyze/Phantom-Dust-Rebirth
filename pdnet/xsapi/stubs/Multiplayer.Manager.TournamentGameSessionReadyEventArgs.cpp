#include "xsapi_pch.h"
#include "Multiplayer.Manager.TournamentGameSessionReadyEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Windows::Foundation::DateTime TournamentGameSessionReadyEventArgs::StartTime()
    {
        PDNET_XSAPI_STUB("TournamentGameSessionReadyEventArgs::StartTime");
    }
}
