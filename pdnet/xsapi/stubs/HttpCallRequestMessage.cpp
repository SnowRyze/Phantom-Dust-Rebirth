#include "xsapi_pch.h"
#include "HttpCallRequestMessage.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    hstring HttpCallRequestMessage::RequestMessageString()
    {
        PDNET_XSAPI_STUB("HttpCallRequestMessage::RequestMessageString");
    }
    com_array<uint8_t> HttpCallRequestMessage::RequestMessageVector()
    {
        PDNET_XSAPI_STUB("HttpCallRequestMessage::RequestMessageVector");
    }
    Microsoft::Xbox::Services::HttpRequestMessageType HttpCallRequestMessage::GetHttpRequestMessageType()
    {
        PDNET_XSAPI_STUB("HttpCallRequestMessage::GetHttpRequestMessageType");
    }
}
