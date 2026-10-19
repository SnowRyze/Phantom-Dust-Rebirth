#pragma once
#include "HttpCallRequestMessage.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct HttpCallRequestMessage : HttpCallRequestMessageT<HttpCallRequestMessage>
    {
        HttpCallRequestMessage() = default;

        hstring RequestMessageString();
        com_array<uint8_t> RequestMessageVector();
        Microsoft::Xbox::Services::HttpRequestMessageType GetHttpRequestMessageType();
    };
}
