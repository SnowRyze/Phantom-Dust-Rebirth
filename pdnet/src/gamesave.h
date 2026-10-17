#pragma once
#include <winrt/Windows.Foundation.h>

namespace pdnet
{
    // Activation factory standing in for Windows.Gaming.XboxLive.Storage.GameSaveProvider.
    // Containers and blobs are plain files under LocalState\pdnet\saves.
    winrt::Windows::Foundation::IActivationFactory CreateGameSaveProviderFactory();
}
