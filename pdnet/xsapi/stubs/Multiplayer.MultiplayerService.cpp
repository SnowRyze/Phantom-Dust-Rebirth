// pdnet-hand: session write/read and subscription state implemented (Stage A: local store); the rest are generated defaults.
#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerService.h"
#include "Multiplayer.WriteSessionResult.h"
#include "Multiplayer.MultiplayerSession.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    winrt::event_token MultiplayerService::MultiplayerSessionChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs> const& __param0)
    {
        return m_sessionChanged.add(__param0);
    }
    void MultiplayerService::MultiplayerSessionChanged(winrt::event_token const& __param0) noexcept
    {
        m_sessionChanged.remove(__param0);
    }
    winrt::event_token MultiplayerService::MultiplayerSubscriptionLost(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSubscriptionLostEventArgs> const& __param0)
    {
        return m_subscriptionLost.add(__param0);
    }
    void MultiplayerService::MultiplayerSubscriptionLost(winrt::event_token const& __param0) noexcept
    {
        m_subscriptionLost.remove(__param0);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        auto result = co_await TryWriteSessionAsync(multiplayerSession, multiplayerSessionWriteMode);
        if (!result.Succeeded()) throw winrt::hresult_error(HTTP_E_STATUS_PRECOND_FAILED, L"pdnet: session write failed");
        co_return result.Session();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        co_await winrt::resume_background();
        auto self = winrt::get_self<implementation::MultiplayerSession>(multiplayerSession);
        bool created = false, deleted = false;
        auto stored = ::pdnet::mpsd::SessionStore::Instance().Write(self->Doc(), multiplayerSessionWriteMode, created, deleted);
        if (!stored)
        {
            ::pdnet::Log(L"xsapi-pdnet: write %s/%s (mode %d) -> refused", self->Doc().templateName.c_str(), self->Doc().sessionName.c_str(), static_cast<int>(multiplayerSessionWriteMode));
            co_return winrt::make<implementation::WriteSessionResult>(false, Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus::HandleNotFound, nullptr);
        }
        ::pdnet::Log(L"xsapi-pdnet: write %s/%s -> %s, %zu member(s), change %llu%s", stored->templateName.c_str(), stored->sessionName.c_str(),
            created ? L"created" : L"updated", stored->members.size(), stored->changeNumber, deleted ? L" (now empty, removed)" : L"");
        auto session = winrt::make<implementation::MultiplayerSession>(self->Context(), stored);
        co_return winrt::make<implementation::WriteSessionResult>(true, stored->writeStatus, session);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::WriteSessionByHandleAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::MultiplayerSession>();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::TryWriteSessionByHandleAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::WriteSessionResult>();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference)
    {
        co_await winrt::resume_background();
        std::wstring key = std::wstring{ sessionReference.ServiceConfigurationId() } + L"|" + std::wstring{ sessionReference.SessionTemplateName() } + L"|" + std::wstring{ sessionReference.SessionName() };
        auto stored = ::pdnet::mpsd::SessionStore::Instance().Get(key);
        ::pdnet::Log(L"xsapi-pdnet: get session %s -> %s", key.c_str(), stored ? L"found" : L"not found");
        if (!stored) throw winrt::hresult_error(HTTP_E_STATUS_NOT_FOUND, L"pdnet: no such session");
        co_return winrt::make<implementation::MultiplayerSession>(m_context, stored);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionByHandleAsync(hstring handleId)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetCurrentSessionByHandleAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::Multiplayer::implementation::MultiplayerSession>();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> MultiplayerService::GetSessionsAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerGetSessionsRequest getSessionsRequest)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetSessionsAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> MultiplayerService::GetSessionsAsync(hstring serviceConfigurationId, hstring sessionTemplateNameFilter, hstring xboxUserIdFilter, hstring keywordFilter, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility visibilityFilter, uint32_t contractVersionFilter, bool includePrivateSessions, bool includeReservations, bool includeInactiveSessions, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetSessionsAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>> MultiplayerService::GetSessionsForUsersFilterAsync(hstring serviceConfigurationId, hstring sessionTemplateNameFilter, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIdsFilter, hstring keywordFilter, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility visibilityFilter, uint32_t contractVersionFilter, bool includePrivateSessions, bool includeReservations, bool includeInactiveSessions, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetSessionsForUsersFilterAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStates>().GetView();
    }
    Windows::Foundation::IAsyncAction MultiplayerService::SetActivityAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::SetActivityAsync"); co_return;
    }
    Windows::Foundation::IAsyncOperation<hstring> MultiplayerService::SetTransferHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference targetSessionReference, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference originSessionReference)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::SetTransferHandleAsync"); co_return hstring{};
    }
    Windows::Foundation::IAsyncAction MultiplayerService::SetSearchHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleRequest searchHandleRequest)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::SetSearchHandleAsync"); co_return;
    }
    Windows::Foundation::IAsyncAction MultiplayerService::ClearActivityAsync(hstring serviceConfigurationId)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::ClearActivityAsync"); co_return;
    }
    Windows::Foundation::IAsyncAction MultiplayerService::ClearSearchHandleAsync(hstring handleId)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::ClearSearchHandleAsync"); co_return;
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> MultiplayerService::SendInvitesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, uint32_t titleId)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::SendInvitesAsync"); co_return winrt::single_threaded_vector<hstring>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<hstring>> MultiplayerService::SendInvitesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, uint32_t titleId, hstring contextStringId, hstring activationContext)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::SendInvitesAsync"); co_return winrt::single_threaded_vector<hstring>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>> MultiplayerService::GetActivitiesForSocialGroupAsync(hstring serviceConfigurationId, hstring socialGroupOwnerXboxUserId, hstring socialGroup)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetActivitiesForSocialGroupAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>> MultiplayerService::GetActivitiesForUsersAsync(hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetActivitiesForUsersAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerActivityDetails>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsync(hstring serviceConfigurationId, hstring sessionTemplateName, hstring orderBy, bool orderAscending, hstring searchFilter)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetSearchHandlesAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>().GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerQuerySearchHandleRequest searchHandleRequest)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::GetSearchHandlesAsync"); co_return winrt::single_threaded_vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>().GetView();
    }
    void MultiplayerService::EnableMultiplayerSubscriptions()
    {
        m_subscriptions = true;
    }
    void MultiplayerService::DisableMultiplayerSubscriptions()
    {
        m_subscriptions = false;
    }
    bool MultiplayerService::MultiplayerSubscriptionsEnabled()
    {
        return m_subscriptions;
    }
}
