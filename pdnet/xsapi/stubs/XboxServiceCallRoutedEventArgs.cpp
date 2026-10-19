#include "xsapi_pch.h"
#include "XboxServiceCallRoutedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    hstring XboxServiceCallRoutedEventArgs::HttpMethod()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::HttpMethod");
    }
    Windows::Foundation::Uri XboxServiceCallRoutedEventArgs::Url()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::Url");
    }
    hstring XboxServiceCallRoutedEventArgs::RequestHeaders()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::RequestHeaders");
    }
    Microsoft::Xbox::Services::HttpCallRequestMessage XboxServiceCallRoutedEventArgs::RequestBody()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::RequestBody");
    }
    uint32_t XboxServiceCallRoutedEventArgs::ResponseCount()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::ResponseCount");
    }
    hstring XboxServiceCallRoutedEventArgs::ResponseHeaders()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::ResponseHeaders");
    }
    hstring XboxServiceCallRoutedEventArgs::ResponseBody()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::ResponseBody");
    }
    hstring XboxServiceCallRoutedEventArgs::ETag()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::ETag");
    }
    hstring XboxServiceCallRoutedEventArgs::Token()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::Token");
    }
    hstring XboxServiceCallRoutedEventArgs::Signature()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::Signature");
    }
    uint32_t XboxServiceCallRoutedEventArgs::HttpStatus()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::HttpStatus");
    }
    hstring XboxServiceCallRoutedEventArgs::FullResponseToString()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::FullResponseToString");
    }
    Windows::Foundation::DateTime XboxServiceCallRoutedEventArgs::RequestTimeUTC()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::RequestTimeUTC");
    }
    Windows::Foundation::DateTime XboxServiceCallRoutedEventArgs::ResponseTimeUTC()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::ResponseTimeUTC");
    }
    Windows::Foundation::TimeSpan XboxServiceCallRoutedEventArgs::ElapsedCallTime()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::ElapsedCallTime");
    }
    hstring XboxServiceCallRoutedEventArgs::XboxUserId()
    {
        PDNET_XSAPI_STUB("XboxServiceCallRoutedEventArgs::XboxUserId");
    }
}
