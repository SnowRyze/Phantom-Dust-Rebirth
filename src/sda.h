#pragma once
#include <string_view>
#include <winrt/Windows.Foundation.h>

namespace pdnet
{
    // Activation factories standing in for Windows.Networking.XboxLive.* (Secure Device Association / Teredo).
    // Returns null for classes we don't replace.
    winrt::Windows::Foundation::IActivationFactory GetXboxLiveNetworkingFactory(std::wstring_view classId);
}
