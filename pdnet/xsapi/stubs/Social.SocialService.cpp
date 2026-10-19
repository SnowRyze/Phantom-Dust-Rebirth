#include "xsapi_pch.h"
#include "Social.SocialService.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    winrt::event_token SocialService::SocialRelationshipChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Social::SocialRelationshipChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("SocialService::SocialRelationshipChanged");
    }
    void SocialService::SocialRelationshipChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("SocialService::SocialRelationshipChanged");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> SocialService::GetSocialRelationshipsAsync()
    {
        PDNET_XSAPI_STUB("SocialService::GetSocialRelationshipsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> SocialService::GetSocialRelationshipsAsync(Microsoft::Xbox::Services::Social::SocialRelationship socialRelationshipFilter)
    {
        PDNET_XSAPI_STUB("SocialService::GetSocialRelationshipsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> SocialService::GetSocialRelationshipsAsync(hstring xboxUserId)
    {
        PDNET_XSAPI_STUB("SocialService::GetSocialRelationshipsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> SocialService::GetSocialRelationshipsAsync(Microsoft::Xbox::Services::Social::SocialRelationship socialRelationshipFilter, uint32_t startIndex, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("SocialService::GetSocialRelationshipsAsync");
    }
    Microsoft::Xbox::Services::Social::SocialRelationshipChangeSubscription SocialService::SubscribeToSocialRelationshipChange(hstring const& xboxUserId)
    {
        PDNET_XSAPI_STUB("SocialService::SubscribeToSocialRelationshipChange");
    }
    void SocialService::UnsubscribeFromSocialRelationshipChange(Microsoft::Xbox::Services::Social::SocialRelationshipChangeSubscription const& subscription)
    {
        PDNET_XSAPI_STUB("SocialService::UnsubscribeFromSocialRelationshipChange");
    }
}
