#include "xsapi_pch.h"
#include "Social.SocialRelationshipChangeEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    hstring SocialRelationshipChangeEventArgs::CallerXboxUserId()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeEventArgs::CallerXboxUserId");
    }
    Microsoft::Xbox::Services::Social::SocialNotificationType SocialRelationshipChangeEventArgs::SocialNotification()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeEventArgs::SocialNotification");
    }
    Windows::Foundation::Collections::IVectorView<hstring> SocialRelationshipChangeEventArgs::XboxUserIds()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeEventArgs::XboxUserIds");
    }
}
