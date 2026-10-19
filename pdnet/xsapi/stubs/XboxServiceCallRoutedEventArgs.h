#pragma once
#include "XboxServiceCallRoutedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxServiceCallRoutedEventArgs : XboxServiceCallRoutedEventArgsT<XboxServiceCallRoutedEventArgs>
    {
        XboxServiceCallRoutedEventArgs() = default;

        hstring HttpMethod();
        Windows::Foundation::Uri Url();
        hstring RequestHeaders();
        Microsoft::Xbox::Services::HttpCallRequestMessage RequestBody();
        uint32_t ResponseCount();
        hstring ResponseHeaders();
        hstring ResponseBody();
        hstring ETag();
        hstring Token();
        hstring Signature();
        uint32_t HttpStatus();
        hstring FullResponseToString();
        Windows::Foundation::DateTime RequestTimeUTC();
        Windows::Foundation::DateTime ResponseTimeUTC();
        Windows::Foundation::TimeSpan ElapsedCallTime();
        hstring XboxUserId();
    };
}
