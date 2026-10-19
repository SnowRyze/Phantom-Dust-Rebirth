#pragma once
#include "XboxLiveHttpCall.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveHttpCall : XboxLiveHttpCallT<XboxLiveHttpCall>
    {
        XboxLiveHttpCall() = default;

        static Microsoft::Xbox::Services::XboxLiveHttpCall CreateXboxLiveHttpCall(Microsoft::Xbox::Services::XboxLiveContextSettings const& settings, hstring const& httpMethod, hstring const& serverName, hstring const& pathQueryFragment);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::XboxLiveHttpCallResponse> GetResponseWithAuth(Microsoft::Xbox::Services::System::XboxLiveUser user, Microsoft::Xbox::Services::HttpCallResponseBodyType httpCallResponseBodyType);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::XboxLiveHttpCallResponse> GetResponseWithoutAuth(Microsoft::Xbox::Services::HttpCallResponseBodyType httpCallResponseBodyType);
        void SetRequestBody(hstring const& value);
        void SetRequestBodyArray(array_view<uint8_t const> requestBodyArray);
        void SetCustomHeader(hstring const& headerName, hstring const& headerValue);
        bool RetryAllowed();
        void RetryAllowed(bool value);
        hstring ContentTypeHeaderValue();
        void ContentTypeHeaderValue(hstring const& value);
        hstring ContractVersionHeaderValue();
        void ContractVersionHeaderValue(hstring const& value);
        hstring ServerName();
        hstring PathQueryFragment();
        hstring HttpMethod();
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct XboxLiveHttpCall : XboxLiveHttpCallT<XboxLiveHttpCall, implementation::XboxLiveHttpCall>
    {
    };
}
