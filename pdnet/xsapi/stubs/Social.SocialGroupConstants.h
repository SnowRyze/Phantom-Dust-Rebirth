#pragma once
#include "Social/SocialGroupConstants.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct SocialGroupConstants : SocialGroupConstantsT<SocialGroupConstants>
    {
        SocialGroupConstants() = default;

        static hstring Favorite();
        static hstring People();
    };
}
namespace winrt::Microsoft::Xbox::Services::Social::factory_implementation
{
    struct SocialGroupConstants : SocialGroupConstantsT<SocialGroupConstants, implementation::SocialGroupConstants>
    {
    };
}
