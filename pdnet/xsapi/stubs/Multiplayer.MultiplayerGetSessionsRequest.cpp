#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerGetSessionsRequest.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    MultiplayerGetSessionsRequest::MultiplayerGetSessionsRequest(hstring const& serviceConfigurationId, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::MultiplayerGetSessionsRequest");
    }
    hstring MultiplayerGetSessionsRequest::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::ServiceConfigurationId");
    }
    uint32_t MultiplayerGetSessionsRequest::MaxItems()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::MaxItems");
    }
    bool MultiplayerGetSessionsRequest::IncludePrivateSessions()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::IncludePrivateSessions");
    }
    void MultiplayerGetSessionsRequest::IncludePrivateSessions(bool value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::IncludePrivateSessions");
    }
    bool MultiplayerGetSessionsRequest::IncludeReservations()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::IncludeReservations");
    }
    void MultiplayerGetSessionsRequest::IncludeReservations(bool value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::IncludeReservations");
    }
    bool MultiplayerGetSessionsRequest::IncludeInactiveSessions()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::IncludeInactiveSessions");
    }
    void MultiplayerGetSessionsRequest::IncludeInactiveSessions(bool value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::IncludeInactiveSessions");
    }
    hstring MultiplayerGetSessionsRequest::XboxUserIdFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::XboxUserIdFilter");
    }
    void MultiplayerGetSessionsRequest::XboxUserIdFilter(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::XboxUserIdFilter");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerGetSessionsRequest::XboxUserIdsFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::XboxUserIdsFilter");
    }
    void MultiplayerGetSessionsRequest::XboxUserIdsFilter(Windows::Foundation::Collections::IVectorView<hstring> const& xboxUserIds)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::XboxUserIdsFilter");
    }
    hstring MultiplayerGetSessionsRequest::KeywordFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::KeywordFilter");
    }
    void MultiplayerGetSessionsRequest::KeywordFilter(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::KeywordFilter");
    }
    hstring MultiplayerGetSessionsRequest::SessionTemplateNameFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::SessionTemplateNameFilter");
    }
    void MultiplayerGetSessionsRequest::SessionTemplateNameFilter(hstring const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::SessionTemplateNameFilter");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility MultiplayerGetSessionsRequest::VisibilityFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::VisibilityFilter");
    }
    void MultiplayerGetSessionsRequest::VisibilityFilter(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility const& value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::VisibilityFilter");
    }
    uint32_t MultiplayerGetSessionsRequest::ContractVersionFilter()
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::ContractVersionFilter");
    }
    void MultiplayerGetSessionsRequest::ContractVersionFilter(uint32_t value)
    {
        PDNET_XSAPI_STUB("MultiplayerGetSessionsRequest::ContractVersionFilter");
    }
}
