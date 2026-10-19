#pragma once
#include "Presence/PresenceService.g.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    struct PresenceService : PresenceServiceT<PresenceService>
    {
        PresenceService() = default;

        winrt::event_token DevicePresenceChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Presence::DevicePresenceChangeEventArgs> const& __param0);
        void DevicePresenceChanged(winrt::event_token const& __param0) noexcept;
        winrt::event_token TitlePresenceChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Presence::TitlePresenceChangeEventArgs> const& __param0);
        void TitlePresenceChanged(winrt::event_token const& __param0) noexcept;
        Windows::Foundation::IAsyncAction SetPresenceAsync(bool isUserActiveInTitle);
        Windows::Foundation::IAsyncAction SetPresenceAsync(bool isUserActiveInTitle, Microsoft::Xbox::Services::Presence::PresenceData presenceData);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Presence::PresenceRecord> GetPresenceAsync(hstring xboxUserId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> GetPresenceForMultipleUsersAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> GetPresenceForMultipleUsersAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceDeviceType> deviceTypes, Windows::Foundation::Collections::IVectorView<uint32_t> titleIds, Microsoft::Xbox::Services::Presence::PresenceDetailLevel detailLevel, bool onlineOnly, bool broadcastingOnly);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> GetPresenceForSocialGroupAsync(hstring socialGroup);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> GetPresenceForSocialGroupAsync(hstring socialGroup, hstring socialGroupOwnerXboxuserId, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceDeviceType> deviceTypes, Windows::Foundation::Collections::IVectorView<uint32_t> titleIds, Microsoft::Xbox::Services::Presence::PresenceDetailLevel detailLevel, bool onlineOnly, bool broadcastingOnly);
        Microsoft::Xbox::Services::Presence::DevicePresenceChangeSubscription SubscribeToDevicePresenceChange(hstring const& xboxUserId);
        void UnsubscribeFromDevicePresenceChange(Microsoft::Xbox::Services::Presence::DevicePresenceChangeSubscription const& subscription);
        Microsoft::Xbox::Services::Presence::TitlePresenceChangeSubscription SubscribeToTitlePresenceChange(hstring const& xboxUserId, uint32_t titleId);
        void UnsubscribeFromTitlePresenceChange(Microsoft::Xbox::Services::Presence::TitlePresenceChangeSubscription const& subscription);
    };
}
