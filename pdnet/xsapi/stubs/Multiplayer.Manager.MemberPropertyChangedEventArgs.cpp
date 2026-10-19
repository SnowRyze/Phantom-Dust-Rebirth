#include "xsapi_pch.h"
#include "Multiplayer.Manager.MemberPropertyChangedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember MemberPropertyChangedEventArgs::Member()
    {
        PDNET_XSAPI_STUB("MemberPropertyChangedEventArgs::Member");
    }
    hstring MemberPropertyChangedEventArgs::Properties()
    {
        PDNET_XSAPI_STUB("MemberPropertyChangedEventArgs::Properties");
    }
}
