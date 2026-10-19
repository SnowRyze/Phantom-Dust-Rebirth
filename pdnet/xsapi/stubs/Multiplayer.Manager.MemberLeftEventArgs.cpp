#include "xsapi_pch.h"
#include "Multiplayer.Manager.MemberLeftEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> MemberLeftEventArgs::Members()
    {
        PDNET_XSAPI_STUB("MemberLeftEventArgs::Members");
    }
}
