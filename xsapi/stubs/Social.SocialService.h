#pragma once
#include "Social/SocialService.g.h"


namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct SocialService : SocialServiceT<SocialService>
    {
        SocialService() = default;

        winrt::event_token SocialRelationshipChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Social::SocialRelationshipChangeEventArgs> const& __param0);
        void SocialRelationshipChanged(winrt::event_token const& __param0) noexcept;
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> GetSocialRelationshipsAsync();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> GetSocialRelationshipsAsync(Microsoft::Xbox::Services::Social::SocialRelationship socialRelationshipFilter);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> GetSocialRelationshipsAsync(hstring xboxUserId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Social::XboxSocialRelationshipResult> GetSocialRelationshipsAsync(Microsoft::Xbox::Services::Social::SocialRelationship socialRelationshipFilter, uint32_t startIndex, uint32_t maxItems);
        Microsoft::Xbox::Services::Social::SocialRelationshipChangeSubscription SubscribeToSocialRelationshipChange(hstring const& xboxUserId);
        void UnsubscribeFromSocialRelationshipChange(Microsoft::Xbox::Services::Social::SocialRelationshipChangeSubscription const& subscription);
    };
}
