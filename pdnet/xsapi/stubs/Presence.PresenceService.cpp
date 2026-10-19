#include "xsapi_pch.h"
#include "Presence.PresenceService.h"


namespace winrt::Microsoft::Xbox::Services::Presence::implementation
{
    winrt::event_token PresenceService::DevicePresenceChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Presence::DevicePresenceChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("PresenceService::DevicePresenceChanged");
    }
    void PresenceService::DevicePresenceChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("PresenceService::DevicePresenceChanged");
    }
    winrt::event_token PresenceService::TitlePresenceChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Presence::TitlePresenceChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("PresenceService::TitlePresenceChanged");
    }
    void PresenceService::TitlePresenceChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("PresenceService::TitlePresenceChanged");
    }
    Windows::Foundation::IAsyncAction PresenceService::SetPresenceAsync(bool isUserActiveInTitle)
    {
        PDNET_XSAPI_STUB("PresenceService::SetPresenceAsync");
    }
    Windows::Foundation::IAsyncAction PresenceService::SetPresenceAsync(bool isUserActiveInTitle, Microsoft::Xbox::Services::Presence::PresenceData presenceData)
    {
        PDNET_XSAPI_STUB("PresenceService::SetPresenceAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Presence::PresenceRecord> PresenceService::GetPresenceAsync(hstring xboxUserId)
    {
        PDNET_XSAPI_STUB("PresenceService::GetPresenceAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> PresenceService::GetPresenceForMultipleUsersAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds)
    {
        PDNET_XSAPI_STUB("PresenceService::GetPresenceForMultipleUsersAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> PresenceService::GetPresenceForMultipleUsersAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceDeviceType> deviceTypes, Windows::Foundation::Collections::IVectorView<uint32_t> titleIds, Microsoft::Xbox::Services::Presence::PresenceDetailLevel detailLevel, bool onlineOnly, bool broadcastingOnly)
    {
        PDNET_XSAPI_STUB("PresenceService::GetPresenceForMultipleUsersAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> PresenceService::GetPresenceForSocialGroupAsync(hstring socialGroup)
    {
        PDNET_XSAPI_STUB("PresenceService::GetPresenceForSocialGroupAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceRecord>> PresenceService::GetPresenceForSocialGroupAsync(hstring socialGroup, hstring socialGroupOwnerXboxuserId, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Presence::PresenceDeviceType> deviceTypes, Windows::Foundation::Collections::IVectorView<uint32_t> titleIds, Microsoft::Xbox::Services::Presence::PresenceDetailLevel detailLevel, bool onlineOnly, bool broadcastingOnly)
    {
        PDNET_XSAPI_STUB("PresenceService::GetPresenceForSocialGroupAsync");
    }
    Microsoft::Xbox::Services::Presence::DevicePresenceChangeSubscription PresenceService::SubscribeToDevicePresenceChange(hstring const& xboxUserId)
    {
        PDNET_XSAPI_STUB("PresenceService::SubscribeToDevicePresenceChange");
    }
    void PresenceService::UnsubscribeFromDevicePresenceChange(Microsoft::Xbox::Services::Presence::DevicePresenceChangeSubscription const& subscription)
    {
        PDNET_XSAPI_STUB("PresenceService::UnsubscribeFromDevicePresenceChange");
    }
    Microsoft::Xbox::Services::Presence::TitlePresenceChangeSubscription PresenceService::SubscribeToTitlePresenceChange(hstring const& xboxUserId, uint32_t titleId)
    {
        PDNET_XSAPI_STUB("PresenceService::SubscribeToTitlePresenceChange");
    }
    void PresenceService::UnsubscribeFromTitlePresenceChange(Microsoft::Xbox::Services::Presence::TitlePresenceChangeSubscription const& subscription)
    {
        PDNET_XSAPI_STUB("PresenceService::UnsubscribeFromTitlePresenceChange");
    }
}
