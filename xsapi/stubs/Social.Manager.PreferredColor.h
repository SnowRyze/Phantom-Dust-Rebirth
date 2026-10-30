#pragma once
#include "Social/Manager/PreferredColor.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    struct PreferredColor : PreferredColorT<PreferredColor>
    {
        PreferredColor() = default;

        hstring PrimaryColor();
        hstring SecondaryColor();
        hstring TertiaryColor();
    };
}
