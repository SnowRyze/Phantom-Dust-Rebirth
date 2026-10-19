#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerActivityDetails.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerActivityDetails::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::SessionReference");
    }
    hstring MultiplayerActivityDetails::HandleId()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::HandleId");
    }
    uint32_t MultiplayerActivityDetails::TitleId()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::TitleId");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility MultiplayerActivityDetails::Visibility()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::Visibility");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction MultiplayerActivityDetails::JoinRestriction()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::JoinRestriction");
    }
    bool MultiplayerActivityDetails::Closed()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::Closed");
    }
    hstring MultiplayerActivityDetails::OwnerXboxUserId()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::OwnerXboxUserId");
    }
    uint32_t MultiplayerActivityDetails::MaxMembersCount()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::MaxMembersCount");
    }
    uint32_t MultiplayerActivityDetails::MembersCount()
    {
        PDNET_XSAPI_STUB("MultiplayerActivityDetails::MembersCount");
    }
}
