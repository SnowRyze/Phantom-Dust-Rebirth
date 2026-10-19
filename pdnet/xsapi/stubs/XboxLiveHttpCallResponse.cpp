#include "xsapi_pch.h"
#include "XboxLiveHttpCallResponse.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    Microsoft::Xbox::Services::HttpCallResponseBodyType XboxLiveHttpCallResponse::BodyType()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::BodyType");
    }
    hstring XboxLiveHttpCallResponse::ResponseBodyString()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ResponseBodyString");
    }
    hstring XboxLiveHttpCallResponse::ResponseBodyJson()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ResponseBodyJson");
    }
    com_array<uint8_t> XboxLiveHttpCallResponse::ResponseBodyVector()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ResponseBodyVector");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> XboxLiveHttpCallResponse::Headers()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::Headers");
    }
    int32_t XboxLiveHttpCallResponse::HttpStatus()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::HttpStatus");
    }
    int32_t XboxLiveHttpCallResponse::ErrorCode()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ErrorCode");
    }
    hstring XboxLiveHttpCallResponse::ErrorMessage()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ErrorMessage");
    }
    hstring XboxLiveHttpCallResponse::ETag()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ETag");
    }
    hstring XboxLiveHttpCallResponse::ResponseDate()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::ResponseDate");
    }
    int64_t XboxLiveHttpCallResponse::RetryAfterInSeconds()
    {
        PDNET_XSAPI_STUB("XboxLiveHttpCallResponse::RetryAfterInSeconds");
    }
}
