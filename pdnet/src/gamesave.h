#pragma once
#include <winrt/Windows.Foundation.h>

namespace pdnet
{
    // Activation factory standing in for Windows.Gaming.XboxLive.Storage.GameSaveProvider.
    // Containers and blobs are plain files under LocalState\pdnet\saves.
    winrt::Windows::Foundation::IActivationFactory CreateGameSaveProviderFactory();

    // Activation factory standing in for Windows.System.Launcher: opening the xbox.com page (the old Help & Support
    // button) imports the player's Xbox cloud save into the local saves instead; everything else goes to Windows.
    winrt::Windows::Foundation::IActivationFactory CreateLauncherFactory();
}
