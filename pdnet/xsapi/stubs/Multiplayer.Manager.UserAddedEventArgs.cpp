#include "xsapi_pch.h"
#include "Multiplayer.Manager.UserAddedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    hstring UserAddedEventArgs::XboxUserId()
    {
        PDNET_XSAPI_STUB("UserAddedEventArgs::XboxUserId");
    }
}
