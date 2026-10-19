#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionReference.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    MultiplayerSessionReference::MultiplayerSessionReference(hstring const& serviceConfigurationId, hstring const& sessionTemplateName, hstring const& sessionName)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionReference::MultiplayerSessionReference");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSessionReference::ParseFromUriPath(hstring const& uriPath)
    {
        PDNET_XSAPI_STUB("MultiplayerSessionReference::ParseFromUriPath");
    }
    hstring MultiplayerSessionReference::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionReference::ServiceConfigurationId");
    }
    hstring MultiplayerSessionReference::SessionTemplateName()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionReference::SessionTemplateName");
    }
    hstring MultiplayerSessionReference::SessionName()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionReference::SessionName");
    }
    hstring MultiplayerSessionReference::ToUriPath()
    {
        PDNET_XSAPI_STUB("MultiplayerSessionReference::ToUriPath");
    }
}
