#pragma once
#include "Social/XboxUserProfile.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct XboxUserProfile : XboxUserProfileT<XboxUserProfile>
    {
        XboxUserProfile() = default;

        hstring ApplicationDisplayName();
        Windows::Foundation::Uri ApplicationDisplayPictureResizeUri();
        hstring GameDisplayName();
        Windows::Foundation::Uri GameDisplayPictureResizeUri();
        hstring Gamerscore();
        hstring Gamertag();
        hstring XboxUserId();
    };
}
