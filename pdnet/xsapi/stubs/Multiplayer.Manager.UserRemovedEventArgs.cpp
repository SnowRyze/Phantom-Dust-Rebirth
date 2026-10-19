#include "xsapi_pch.h"
#include "Multiplayer.Manager.UserRemovedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    hstring UserRemovedEventArgs::XboxUserId()
    {
        PDNET_XSAPI_STUB("UserRemovedEventArgs::XboxUserId");
    }
}
