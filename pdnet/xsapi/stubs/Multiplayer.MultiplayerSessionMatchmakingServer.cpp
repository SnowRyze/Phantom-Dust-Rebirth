#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionMatchmakingServer.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MatchmakingStatus MultiplayerSessionMatchmakingServer::Status()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMatchmakingServer::Status");
    }
    hstring MultiplayerSessionMatchmakingServer::StatusDetails()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMatchmakingServer::StatusDetails");
    }
    Windows::Foundation::TimeSpan MultiplayerSessionMatchmakingServer::TypicalWait()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMatchmakingServer::TypicalWait");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSessionMatchmakingServer::TargetSessionRef()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionMatchmakingServer::TargetSessionRef");
    }
}
