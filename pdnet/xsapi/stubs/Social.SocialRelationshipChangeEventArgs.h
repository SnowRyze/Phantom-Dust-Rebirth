#pragma once
#include "Social/SocialRelationshipChangeEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct SocialRelationshipChangeEventArgs : SocialRelationshipChangeEventArgsT<SocialRelationshipChangeEventArgs>
    {
        SocialRelationshipChangeEventArgs() = default;

        hstring CallerXboxUserId();
        Microsoft::Xbox::Services::Social::SocialNotificationType SocialNotification();
        Windows::Foundation::Collections::IVectorView<hstring> XboxUserIds();
    };
}
