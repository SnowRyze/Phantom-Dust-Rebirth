#include "xsapi_pch.h"
#include "XboxLiveHttpCall.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    Microsoft::Xbox::Services::XboxLiveHttpCall XboxLiveHttpCall::CreateXboxLiveHttpCall(Microsoft::Xbox::Services::XboxLiveContextSettings const& settings, hstring const& httpMethod, hstring const& serverName, hstring const& pathQueryFragment)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::CreateXboxLiveHttpCall");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::XboxLiveHttpCallResponse> XboxLiveHttpCall::GetResponseWithAuth(Microsoft::Xbox::Services::System::XboxLiveUser user, Microsoft::Xbox::Services::HttpCallResponseBodyType httpCallResponseBodyType)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::GetResponseWithAuth");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::XboxLiveHttpCallResponse> XboxLiveHttpCall::GetResponseWithoutAuth(Microsoft::Xbox::Services::HttpCallResponseBodyType httpCallResponseBodyType)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::GetResponseWithoutAuth");
    }
    void XboxLiveHttpCall::SetRequestBody(hstring const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::SetRequestBody");
    }
    void XboxLiveHttpCall::SetRequestBodyArray(array_view<uint8_t const> requestBodyArray)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::SetRequestBodyArray");
    }
    void XboxLiveHttpCall::SetCustomHeader(hstring const& headerName, hstring const& headerValue)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::SetCustomHeader");
    }
    bool XboxLiveHttpCall::RetryAllowed()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::RetryAllowed");
    }
    void XboxLiveHttpCall::RetryAllowed(bool value)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::RetryAllowed");
    }
    hstring XboxLiveHttpCall::ContentTypeHeaderValue()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::ContentTypeHeaderValue");
    }
    void XboxLiveHttpCall::ContentTypeHeaderValue(hstring const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::ContentTypeHeaderValue");
    }
    hstring XboxLiveHttpCall::ContractVersionHeaderValue()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::ContractVersionHeaderValue");
    }
    void XboxLiveHttpCall::ContractVersionHeaderValue(hstring const& value)
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::ContractVersionHeaderValue");
    }
    hstring XboxLiveHttpCall::ServerName()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::ServerName");
    }
    hstring XboxLiveHttpCall::PathQueryFragment()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::PathQueryFragment");
    }
    hstring XboxLiveHttpCall::HttpMethod()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCall::HttpMethod");
    }
}
