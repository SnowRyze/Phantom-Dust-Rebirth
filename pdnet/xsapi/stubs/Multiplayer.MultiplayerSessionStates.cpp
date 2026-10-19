#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionStates.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Windows::Foundation::DateTime MultiplayerSessionStates::StartTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::StartTime");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSessionStates::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::SessionReference");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStatus MultiplayerSessionStates::Status()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::Status");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility MultiplayerSessionStates::Visibility()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::Visibility");
    }
    bool MultiplayerSessionStates::IsMyTurn()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::IsMyTurn");
    }
    hstring MultiplayerSessionStates::XboxUserId()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::XboxUserId");
    }
    uint32_t MultiplayerSessionStates::AcceptedMemberCount()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::AcceptedMemberCount");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction MultiplayerSessionStates::JoinRestriction()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::JoinRestriction");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSessionStates::Keywords()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionStates::Keywords");
    }
}
