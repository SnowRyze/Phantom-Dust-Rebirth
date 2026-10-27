#pragma once
// Controllers that Windows.Gaming.Input.Gamepad doesn't list (PlayStation, Nintendo, generic HID pads) are read by
// pdlaunch with SDL3 and handed to pdnet in the game through LocalState\pdnet\pads.bin, a file both processes map (the
// game's sandbox can't open those controllers itself). Values are already in Xbox layout: buttons are
// Windows.Gaming.Input.GamepadButtons bits, sticks -1..1 (right and up positive), triggers 0..1.
// See pdlaunch.cpp (writer) and gamepads.cpp (the game's Gamepad class).
#include <cstdint>

namespace pdnet::pads
{
    constexpr uint32_t kMagic = 0x44504450; // "PDPD"
    constexpr uint32_t kVersion = 1;
    constexpr int kMaxPads = 8;
    constexpr const wchar_t* kFile = L"\\pdnet\\pads.bin"; // under LocalState

    struct Pad
    {
        uint32_t id;      // pdlaunch's id for the pad (never reused while it runs); 0 = no pad in this slot
        uint32_t type;    // SDL_GamepadType (PlayStation, Nintendo, ...)
        uint32_t buttons; // GamepadButtons bits
        float leftX, leftY, rightX, rightY, leftTrigger, rightTrigger;
        char name[64];
    };

    struct Rumble
    {
        uint32_t id; // the pad it is for (Pad::id)
        float leftMotor, rightMotor, leftTrigger, rightTrigger;
    };

    struct Shared
    {
        uint32_t magic, version;
        volatile uint32_t sequence;  // pdlaunch: odd while it writes pads[]
        volatile uint32_t heartbeat; // pdlaunch: GetTickCount() of its last write (no fresh heartbeat: no pads)
        Pad pads[kMaxPads];
        volatile uint32_t rumbleSequence; // pdnet: odd while it writes rumble[]
        Rumble rumble[kMaxPads];
    };

    // Windows.Gaming.Input.GamepadButtons
    enum : uint32_t
    {
        kMenu = 0x1, kView = 0x2, kA = 0x4, kB = 0x8, kX = 0x10, kY = 0x20,
        kDPadUp = 0x40, kDPadDown = 0x80, kDPadLeft = 0x100, kDPadRight = 0x200,
        kLeftShoulder = 0x400, kRightShoulder = 0x800, kLeftThumbstick = 0x1000, kRightThumbstick = 0x2000,
        kPaddle1 = 0x4000, kPaddle2 = 0x8000, kPaddle3 = 0x10000, kPaddle4 = 0x20000,
    };
}
