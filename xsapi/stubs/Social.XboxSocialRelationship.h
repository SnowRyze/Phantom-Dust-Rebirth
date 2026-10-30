#pragma once
#include "Social/XboxSocialRelationship.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct XboxSocialRelationship : XboxSocialRelationshipT<XboxSocialRelationship>
    {
        XboxSocialRelationship() = default;

        hstring XboxUserId();
        bool IsFavorite();
        bool IsFollowingCaller();
        Windows::Foundation::Collections::IVectorView<hstring> SocialNetworks();
    };
}
