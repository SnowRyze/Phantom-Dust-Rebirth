#include "xsapi_pch.h"
#include "Multiplayer.Manager.MultiplayerEvent.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    int32_t MultiplayerEvent::ErrorCode()
    {
        PDNET_XSAPI_STUB("MultiplayerEvent::ErrorCode");
    }
    hstring MultiplayerEvent::ErrorMessage()
    {
        PDNET_XSAPI_STUB("MultiplayerEvent::ErrorMessage");
    }
    Windows::Foundation::IInspectable MultiplayerEvent::Context()
    {
        PDNET_XSAPI_STUB("MultiplayerEvent::Context");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerEventType MultiplayerEvent::EventType()
    {
        PDNET_XSAPI_STUB("MultiplayerEvent::EventType");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerEventArgs MultiplayerEvent::EventArgs()
    {
        PDNET_XSAPI_STUB("MultiplayerEvent::EventArgs");
    }
    Microsoft::Xbox::Services::Multiplayer::Manager::MultiplayerSessionType MultiplayerEvent::SessionType()
    {
        PDNET_XSAPI_STUB("MultiplayerEvent::SessionType");
    }
}
