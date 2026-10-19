#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerService.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    winrt::event_token MultiplayerService::MultiplayerSessionChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("MultiplayerService::MultiplayerSessionChanged");
    }
    void MultiplayerService::MultiplayerSessionChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("MultiplayerService::MultiplayerSessionChanged");
    }
    winrt::event_token MultiplayerService::MultiplayerSubscriptionLost(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSubscriptionLostEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("MultiplayerService::MultiplayerSubscriptionLost");
    }
    void MultiplayerService::MultiplayerSubscriptionLost(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("MultiplayerService::MultiplayerSubscriptionLost");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        PDNET_XSAPI_STUB("MultiplayerService::WriteSessionAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        PDNET_XSAPI_STUB("MultiplayerService::TryWriteSessionAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        PDNET_XSAPI_STUB("MultiplayerService::WriteSessionByHandleAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        PDNET_XSAPI_STUB("MultiplayerService::TryWriteSessionByHandleAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetCurrentSessionAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionByHandleAsync(hstring handleId)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetCurrentSessionByHandleAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> MultiplayerService::GetSessionsAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerGetSessionsRequest getSessionsRequest)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetSessionsAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> MultiplayerService::GetSessionsAsync(hstring serviceConfigurationId, hstring sessionTemplateNameFilter, hstring xboxUserIdFilter, hstring keywordFilter, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility visibilityFilter, uint32_t contractVersionFilter, bool includePrivateSessions, bool includeReservations, bool includeInactiveSessions, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetSessionsAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> MultiplayerService::GetSessionsForUsersFilterAsync(hstring serviceConfigurationId, hstring sessionTemplateNameFilter, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIdsFilter, hstring keywordFilter, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility visibilityFilter, uint32_t contractVersionFilter, bool includePrivateSessions, bool includeReservations, bool includeInactiveSessions, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetSessionsForUsersFilterAsync");
    }
    Windows::Foundation::IAsyncAction MultiplayerService::SetActivityAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference)
    {
        PDNET_XSAPI_STUB("MultiplayerService::SetActivityAsync");
    }
    Windows::Foundation::IAsyncOperation<hstring> MultiplayerService::SetTransferHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference targetSessionReference, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference originSessionReference)
    {
        PDNET_XSAPI_STUB("MultiplayerService::SetTransferHandleAsync");
    }
    Windows::Foundation::IAsyncAction MultiplayerService::SetSearchHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleRequest searchHandleRequest)
    {
        PDNET_XSAPI_STUB("MultiplayerService::SetSearchHandleAsync");
    }
    Windows::Foundation::IAsyncAction MultiplayerService::ClearActivityAsync(hstring serviceConfigurationId)
    {
        PDNET_XSAPI_STUB("MultiplayerService::ClearActivityAsync");
    }
    Windows::Foundation::IAsyncAction MultiplayerService::ClearSearchHandleAsync(hstring handleId)
    {
        PDNET_XSAPI_STUB("MultiplayerService::ClearSearchHandleAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> MultiplayerService::SendInvitesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, uint32_t titleId)
    {
        PDNET_XSAPI_STUB("MultiplayerService::SendInvitesAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> MultiplayerService::SendInvitesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, uint32_t titleId, hstring contextStringId, hstring activationContext)
    {
        PDNET_XSAPI_STUB("MultiplayerService::SendInvitesAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>> MultiplayerService::GetActivitiesForSocialGroupAsync(hstring serviceConfigurationId, hstring socialGroupOwnerXboxUserId, hstring socialGroup)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetActivitiesForSocialGroupAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>> MultiplayerService::GetActivitiesForUsersAsync(hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetActivitiesForUsersAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsync(hstring serviceConfigurationId, hstring sessionTemplateName, hstring orderBy, bool orderAscending, hstring searchFilter)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetSearchHandlesAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerQuerySearchHandleRequest searchHandleRequest)
    {
        PDNET_XSAPI_STUB("MultiplayerService::GetSearchHandlesAsync");
    }
    void MultiplayerService::EnableMultiplayerSubscriptions()
    {
        PDNET_XSAPI_STUB("MultiplayerService::EnableMultiplayerSubscriptions");
    }
    void MultiplayerService::DisableMultiplayerSubscriptions()
    {
        PDNET_XSAPI_STUB("MultiplayerService::DisableMultiplayerSubscriptions");
    }
    bool MultiplayerService::MultiplayerSubscriptionsEnabled()
    {
        PDNET_XSAPI_STUB("MultiplayerService::MultiplayerSubscriptionsEnabled");
    }
}
