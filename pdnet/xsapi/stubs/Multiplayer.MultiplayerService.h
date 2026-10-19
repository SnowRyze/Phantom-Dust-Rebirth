#pragma once
#include "Multiplayer/MultiplayerService.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerService : MultiplayerServiceT<MultiplayerService>
    {
        MultiplayerService() = default;

        winrt::event_token MultiplayerSessionChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs> const& __param0);
        void MultiplayerSessionChanged(winrt::event_token const& __param0) noexcept;
        winrt::event_token MultiplayerSubscriptionLost(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSubscriptionLostEventArgs> const& __param0);
        void MultiplayerSubscriptionLost(winrt::event_token const& __param0) noexcept;
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> WriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> TryWriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> WriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> TryWriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> GetCurrentSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> GetCurrentSessionByHandleAsync(hstring handleId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> GetSessionsAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerGetSessionsRequest getSessionsRequest);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> GetSessionsAsync(hstring serviceConfigurationId, hstring sessionTemplateNameFilter, hstring xboxUserIdFilter, hstring keywordFilter, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility visibilityFilter, uint32_t contractVersionFilter, bool includePrivateSessions, bool includeReservations, bool includeInactiveSessions, uint32_t maxItems);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> GetSessionsForUsersFilterAsync(hstring serviceConfigurationId, hstring sessionTemplateNameFilter, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIdsFilter, hstring keywordFilter, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility visibilityFilter, uint32_t contractVersionFilter, bool includePrivateSessions, bool includeReservations, bool includeInactiveSessions, uint32_t maxItems);
        Windows::Foundation::IAsyncAction SetActivityAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference);
        Windows::Foundation::IAsyncOperation<hstring> SetTransferHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference targetSessionReference, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference originSessionReference);
        Windows::Foundation::IAsyncAction SetSearchHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleRequest searchHandleRequest);
        Windows::Foundation::IAsyncAction ClearActivityAsync(hstring serviceConfigurationId);
        Windows::Foundation::IAsyncAction ClearSearchHandleAsync(hstring handleId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> SendInvitesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, uint32_t titleId);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> SendInvitesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, uint32_t titleId, hstring contextStringId, hstring activationContext);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>> GetActivitiesForSocialGroupAsync(hstring serviceConfigurationId, hstring socialGroupOwnerXboxUserId, hstring socialGroup);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>> GetActivitiesForUsersAsync(hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> GetSearchHandlesAsync(hstring serviceConfigurationId, hstring sessionTemplateName, hstring orderBy, bool orderAscending, hstring searchFilter);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> GetSearchHandlesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerQuerySearchHandleRequest searchHandleRequest);
        void EnableMultiplayerSubscriptions();
        void DisableMultiplayerSubscriptions();
        bool MultiplayerSubscriptionsEnabled();

    private:
        winrt::event<Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs>> m_sessionChanged;
        winrt::event<Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSubscriptionLostEventArgs>> m_subscriptionLost;
        Microsoft::Xbox::Services::XboxLiveContext m_context{ nullptr };
        std::atomic<bool> m_subscriptions{ false };
    };
}
