#pragma once
#include "XboxLiveHttpCallResponse.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveHttpCallResponse : XboxLiveHttpCallResponseT<XboxLiveHttpCallResponse>
    {
        XboxLiveHttpCallResponse() = default;

        Microsoft::Xbox::Services::HttpCallResponseBodyType BodyType();
        hstring ResponseBodyString();
        hstring ResponseBodyJson();
        com_array<uint8_t> ResponseBodyVector();
        Windows::Foundation::Collections::IMapView<hstring, hstring> Headers();
        int32_t HttpStatus();
        int32_t ErrorCode();
        hstring ErrorMessage();
        hstring ETag();
        hstring ResponseDate();
        int64_t RetryAfterInSeconds();
    };
}
