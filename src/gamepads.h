#pragma once

namespace pdnet
{
    // The game's Windows.Gaming.Input.Gamepad class factory: the real one's pads (Xbox and XInput controllers) plus a
    // virtual pad for each controller pdlaunch reads with SDL3 (PlayStation, Nintendo, generic), see padshare.h.
    // `real` is the OS factory; the result is an IActivationFactory that also answers IGamepadStatics / IGamepadStatics2.
    winrt::Windows::Foundation::IInspectable CreateGamepadFactory(const winrt::Windows::Foundation::IActivationFactory& real);
}
