#include "xsapi_pch.h"
#include "Multiplayer.Manager.MemberJoinedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerMember> MemberJoinedEventArgs::Members()
    {
        PDNET_XSAPI_STUB("MemberJoinedEventArgs::Members");
    }
}
