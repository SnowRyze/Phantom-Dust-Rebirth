#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerQuerySearchHandleRequest.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    MultiplayerQuerySearchHandleRequest::MultiplayerQuerySearchHandleRequest(hstring const& serviceConfigurationId, hstring const& sessionTemplateName)
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::MultiplayerQuerySearchHandleRequest");
    }
    hstring MultiplayerQuerySearchHandleRequest::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::ServiceConfigurationId");
    }
    hstring MultiplayerQuerySearchHandleRequest::SessionTemplateName()
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::SessionTemplateName");
    }
    hstring MultiplayerQuerySearchHandleRequest::OrderBy()
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::OrderBy");
    }
    void MultiplayerQuerySearchHandleRequest::OrderBy(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::OrderBy");
    }
    bool MultiplayerQuerySearchHandleRequest::OrderAscending()
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::OrderAscending");
    }
    void MultiplayerQuerySearchHandleRequest::OrderAscending(bool value)
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::OrderAscending");
    }
    hstring MultiplayerQuerySearchHandleRequest::SearchFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::SearchFilter");
    }
    void MultiplayerQuerySearchHandleRequest::SearchFilter(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::SearchFilter");
    }
    hstring MultiplayerQuerySearchHandleRequest::SocialGroup()
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::SocialGroup");
    }
    void MultiplayerQuerySearchHandleRequest::SocialGroup(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerQuerySearchHandleRequest::SocialGroup");
    }
}
