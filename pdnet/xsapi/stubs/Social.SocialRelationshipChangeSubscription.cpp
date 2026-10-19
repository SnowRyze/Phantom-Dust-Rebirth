#include "xsapi_pch.h"
#include "Social.SocialRelationshipChangeSubscription.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivitySubscriptionState SocialRelationshipChangeSubscription::State()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeSubscription::State");
    }
    hstring SocialRelationshipChangeSubscription::ResourceUri()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeSubscription::ResourceUri");
    }
    uint32_t SocialRelationshipChangeSubscription::SubscriptionId()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeSubscription::SubscriptionId");
    }
    hstring SocialRelationshipChangeSubscription::XboxUserId()
    {
        PDNET_XSAPI_STUB("SocialRelationshipChangeSubscription::XboxUserId");
    }
}
