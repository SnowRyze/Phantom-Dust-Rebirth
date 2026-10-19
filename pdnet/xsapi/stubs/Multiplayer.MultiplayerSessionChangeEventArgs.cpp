#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSessionChangeEventArgs::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionChangeEventArgs::SessionReference");
    }
    hstring MultiplayerSessionChangeEventArgs::Branch()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionChangeEventArgs::Branch");
    }
    uint64_t MultiplayerSessionChangeEventArgs::ChangeNumber()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionChangeEventArgs::ChangeNumber");
    }
}
