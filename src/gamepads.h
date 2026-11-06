#pragma once

namespace pdnet
{
    // The game's Windows.Gaming.Input.Gamepad class factory: the real one's pads (Xbox and XInput controllers) plus a
    // virtual pad for each controller pdlaunch reads with SDL3 (PlayStation, Nintendo, generic), see padshare.h.
    // `real` is the OS factory; the result is an IActivationFactory that also answers IGamepadStatics / IGamepadStatics2.
    winrt::Windows::Foundation::IInspectable CreateGamepadFactory(const winrt::Windows::Foundation::IActivationFactory& real);

    // True while any controller (Xbox pads from Windows, SDL pads from pdlaunch) holds one of `buttons`
    // (Windows.Gaming.Input.GamepadButtons bits, e.g. 0x800 = RightShoulder). False before the game made its factory.
    bool AnyPadHolds(uint32_t buttons);

    // The keyboard as pdlaunch reads it (padshare.h): false when it hasn't reported lately. keys: 256 flags by virtual-key code.
    bool KeysHeld(uint8_t* keys);
}
