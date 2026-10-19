#include "xsapi_pch.h"
#include "Multiplayer.Manager.HostChangedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember HostChangedEventArgs::HostMember()
    {
        PDNET_XSAPI_STUB("HostChangedEventArgs::HostMember");
    }
}
