// pdnet-hand: session write/read, search handles and change events implemented (stage B: EOS lobbies, see mpsd.h); the rest are generated defaults.
#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerService.h"
#include "Multiplayer.WriteSessionResult.h"
#include "Multiplayer.MultiplayerSession.h"
#include "Multiplayer.MultiplayerSessionReference.h"
#include "Multiplayer.MultiplayerSessionChangeEventArgs.h"
#include "Multiplayer.MultiplayerSubscriptionLostEventArgs.h"
#include "Multiplayer.MultiplayerSearchHandleDetails.h"
#include <algorithm>
#include "../repeatable.h"
#include "../../src/gamepatches.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    namespace
    {
        // Every MultiplayerService the game made; the session store raises its events through them.
        std::mutex g_servicesLock;
        std::vector<winrt::weak_ref<MultiplayerService>> g_services;

        template <typename Fn>
        void ForEachService(Fn fn)
        {
            std::vector<winrt::com_ptr<MultiplayerService>> alive;
            {
                std::lock_guard guard(g_servicesLock);
                std::erase_if(g_services, [&](auto& weak)
                {
                    auto strong = weak.get();
                    if (strong) alive.push_back(strong);
                    return !strong;
                });
            }
            for (auto& s : alive) fn(*s);
        }

        std::wstring KeyOf(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference const& r)
        {
            return std::wstring{ r.ServiceConfigurationId() } + L"|" + std::wstring{ r.SessionTemplateName() } + L"|" + std::wstring{ r.SessionName() };
        }
    }

    void MultiplayerService::Register()
    {
        if (m_registered.exchange(true)) return;
        static std::once_flag once;
        std::call_once(once, []
        {
            ::pdnet::mpsd::SessionStore::Instance().SetHandlers(
                [](::pdnet::mpsd::SessionPtr session, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes changes)
                {
                    ForEachService([&](MultiplayerService& s) { s.RaiseSessionChanged(session, changes); });
                },
                []
                {
                    ForEachService([](MultiplayerService& s) { s.RaiseSubscriptionLost(); });
                });
        });
        std::lock_guard guard(g_servicesLock);
        g_services.push_back(get_weak());
    }

    void MultiplayerService::RaiseSessionChanged(::pdnet::mpsd::SessionPtr const& session, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes changes)
    {
        if (!m_subscriptions) return;
        int subscribed = static_cast<int>(session->subscribedChanges);
        if (subscribed && !(subscribed & static_cast<int>(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeTypes::Everything)) && !(subscribed & static_cast<int>(changes))) return;
        auto reference = winrt::make<implementation::MultiplayerSessionReference>(hstring{ session->scid }, hstring{ session->templateName }, hstring{ session->sessionName });
        m_sessionChanged(*this, winrt::make<implementation::MultiplayerSessionChangeEventArgs>(reference, hstring{ session->correlationId }, session->changeNumber));
    }

    void MultiplayerService::RaiseSubscriptionLost()
    {
        if (!m_subscriptions) return;
        m_subscriptions = false;
        m_subscriptionLost(*this, winrt::make<implementation::MultiplayerSubscriptionLostEventArgs>());
    }

    winrt::event_token MultiplayerService::MultiplayerSessionChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionChangeEventArgs> const& __param0)
    {
        Register();
        return m_sessionChanged.add(__param0);
    }
    void MultiplayerService::MultiplayerSessionChanged(winrt::event_token const& __param0) noexcept
    {
        m_sessionChanged.remove(__param0);
    }
    winrt::event_token MultiplayerService::MultiplayerSubscriptionLost(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::Multiplayer::MultiplayerSubscriptionLostEventArgs> const& __param0)
    {
        Register();
        return m_subscriptionLost.add(__param0);
    }
    void MultiplayerService::MultiplayerSubscriptionLost(winrt::event_token const& __param0) noexcept
    {
        m_subscriptionLost.remove(__param0);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionAsyncImpl(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        auto result = co_await TryWriteSessionAsync(multiplayerSession, multiplayerSessionWriteMode);
        if (!result.Succeeded()) throw winrt::hresult_error(HTTP_E_STATUS_PRECOND_FAILED, L"pdnet: session write failed");
        co_return result.Session();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionAsyncImpl(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        co_await winrt::resume_background();
        auto self = winrt::get_self<implementation::MultiplayerSession>(multiplayerSession);
        bool created = false, deleted = false;
        auto status = Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus::Unknown;
        auto stored = ::pdnet::mpsd::SessionStore::Instance().Write(self->Doc(), multiplayerSessionWriteMode, created, deleted, status);
        if (!stored)
        {
            ::pdnet::Log(L"xsapi-pdnet: write %s/%s (mode %d) -> refused (status %d)", self->Doc().templateName.c_str(), self->Doc().sessionName.c_str(), static_cast<int>(multiplayerSessionWriteMode), static_cast<int>(status));
            co_return winrt::make<implementation::WriteSessionResult>(false, status, nullptr);
        }
        ::pdnet::Log(L"xsapi-pdnet: write %s/%s -> %s, %zu member(s), change %llu%s", stored->templateName.c_str(), stored->sessionName.c_str(),
            created ? L"created" : L"updated", stored->members.size(), stored->changeNumber, deleted ? L" (left or empty, removed)" : stored->shared ? L" [shared]" : L"");
        auto session = winrt::make<implementation::MultiplayerSession>(self->Context(), stored);
        co_return winrt::make<implementation::WriteSessionResult>(true, stored->writeStatus, session);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionByHandleAsyncImpl(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        auto result = co_await TryWriteSessionByHandleAsync(multiplayerSession, multiplayerSessionWriteMode, handleId);
        if (!result.Succeeded()) throw winrt::hresult_error(HTTP_E_STATUS_NOT_FOUND, L"pdnet: session write by handle failed");
        co_return result.Session();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionByHandleAsyncImpl(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        co_await winrt::resume_background();
        auto target = ::pdnet::mpsd::SessionStore::Instance().GetByHandle(std::wstring{ handleId });
        ::pdnet::Log(L"xsapi-pdnet: write by handle %s -> %s", handleId.c_str(), target ? target->Key().c_str() : L"no such session");
        if (!target) co_return winrt::make<implementation::WriteSessionResult>(false, Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus::HandleNotFound, nullptr);
        auto self = winrt::get_self<implementation::MultiplayerSession>(multiplayerSession);
        ::pdnet::mpsd::SessionDoc written = self->Doc();
        written.scid = target->scid;
        written.templateName = target->templateName;
        written.sessionName = target->sessionName;
        auto session = winrt::make<implementation::MultiplayerSession>(self->Context(), std::make_shared<::pdnet::mpsd::SessionDoc>(std::move(written)));
        co_return co_await TryWriteSessionAsync(session, multiplayerSessionWriteMode);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionAsyncImpl(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference)
    {
        co_await winrt::resume_background();
        std::wstring key = KeyOf(sessionReference);
        auto stored = ::pdnet::mpsd::SessionStore::Instance().Get(key);
        ::pdnet::Log(L"xsapi-pdnet: get session %s -> %s", key.c_str(), stored ? L"found" : L"not found");
        if (!stored) throw winrt::hresult_error(HTTP_E_STATUS_NOT_FOUND, L"pdnet: no such session");
        co_return winrt::make<implementation::MultiplayerSession>(m_context, stored);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionByHandleAsyncImpl(hstring handleId)
    {
        co_await winrt::resume_background();
        auto stored = ::pdnet::mpsd::SessionStore::Instance().GetByHandle(std::wstring{ handleId });
        ::pdnet::Log(L"xsapi-pdnet: get session by handle %s -> %s", handleId.c_str(), stored ? stored->Key().c_str() : L"not found");
        if (!stored) throw winrt::hresult_error(HTTP_E_STATUS_NOT_FOUND, L"pdnet: no such session");
        co_return winrt::make<implementation::MultiplayerSession>(m_context, stored);
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
        co_return; // Xbox presence: nothing to do without Xbox Live
    }
    Windows::Foundation::IAsyncOperation<hstring> MultiplayerService::SetTransferHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference targetSessionReference, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference originSessionReference)
    {
        PDNET_XSAPI_STUB_DEFAULT("MultiplayerService::SetTransferHandleAsync"); co_return hstring{};
    }
    Windows::Foundation::IAsyncAction MultiplayerService::SetSearchHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleRequest searchHandleRequest)
    {
        co_await winrt::resume_background();
        auto reference = searchHandleRequest.SessionReference();
        if (!reference) co_return;
        ::pdnet::mpsd::SearchHandle handle;
        if (auto tags = searchHandleRequest.Tags()) for (auto const& t : tags) handle.tags.emplace_back(t);
        if (auto numbers = searchHandleRequest.NumbersMetadata()) for (auto const& kv : numbers) handle.numbers[std::wstring{ kv.Key() }] = kv.Value();
        if (auto strings = searchHandleRequest.StringsMetadata()) for (auto const& kv : strings) handle.strings[std::wstring{ kv.Key() }] = std::wstring{ kv.Value() };
        // The lobby's Rating option is Unrated / Rated (ratinglimit 0 / 1, see src/scores.h), not the original
        // "only players within 200 of the host's rating": everyone may join either kind.
        if (handle.numbers.count(L"ratingmin")) handle.numbers[L"ratingmin"] = 0;
        if (handle.numbers.count(L"ratingmax")) handle.numbers[L"ratingmax"] = 1000000;
        std::wstring summary;
        for (auto& [k, v] : handle.numbers) summary += L" " + k + L"=" + std::to_wstring(v);
        for (auto& [k, v] : handle.strings) summary += L" " + k + L"='" + v + L"'";
        bool ok = ::pdnet::mpsd::SessionStore::Instance().SetSearchHandle(KeyOf(reference), std::move(handle));
        ::pdnet::Log(L"xsapi-pdnet: search handle for %s/%s:%s%s", reference.SessionTemplateName().c_str(), reference.SessionName().c_str(), summary.c_str(), ok ? L"" : L" -> unknown session, ignored");
    }
    Windows::Foundation::IAsyncAction MultiplayerService::ClearActivityAsync(hstring serviceConfigurationId)
    {
        co_return;
    }
    Windows::Foundation::IAsyncAction MultiplayerService::ClearSearchHandleAsync(hstring handleId)
    {
        co_await winrt::resume_background();
        bool ok = ::pdnet::mpsd::SessionStore::Instance().ClearSearchHandle(std::wstring{ handleId });
        ::pdnet::Log(L"xsapi-pdnet: clear search handle %s%s", handleId.c_str(), ok ? L"" : L" -> not found");
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
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsyncImpl(hstring serviceConfigurationId, hstring sessionTemplateName, hstring orderBy, bool orderAscending, hstring searchFilter)
    {
        co_await winrt::resume_background();
        auto all = ::pdnet::mpsd::SessionStore::Instance().SearchHandles(std::wstring{ serviceConfigurationId }, std::wstring{ sessionTemplateName });
        std::wstring filter{ searchFilter };
        // Quick Join (the menu's former Quick Match, the only search asking for "numbers/gamerule eq 2"): any open
        // lobby, of any game type and rule, as long as this player can play its rule (a usable arsenal, or Quick, which
        // needs none - so without one only Quick lobbies are found). The game joins the first result.
        uint32_t joinable = 0;
        for (const wchar_t* clause : { L" and numbers/gamerule eq 2", L"numbers/gamerule eq 2 and " })
            if (size_t at = filter.find(clause); at != std::wstring::npos)
            {
                filter.erase(at, wcslen(clause));
                joinable = ::pdnet::gamepatches::JoinableRules();
                break;
            }
        std::vector<::pdnet::mpsd::SessionPtr> matched;
        for (auto& doc : all)
        {
            if (!filter.empty() && !::pdnet::mpsd::MatchesFilter(*doc->search, filter)) continue;
            if (joinable)
            {
                int rule = 0;
                for (auto& [k, v] : doc->search->numbers)
                    if (_wcsicmp(k.c_str(), L"gamerule") == 0) rule = static_cast<int>(v);
                if (rule < 0 || rule > 11 || !(joinable & (1u << rule))) continue;
            }
            matched.push_back(doc);
        }
        if (joinable) ::pdnet::Log(L"xsapi-pdnet: Quick Join - any game type; rules this player can play: mask 0x%03X (Quick is bit 2)", joinable);

        std::wstring order{ orderBy };
        for (auto& c : order) c = static_cast<wchar_t>(towlower(c));
        auto sortKey = [&](const ::pdnet::mpsd::SessionDoc& d) -> double
        {
            if (order.rfind(L"numbers/", 0) == 0)
                for (auto& [k, v] : d.search->numbers)
                    if (_wcsicmp(k.c_str(), order.c_str() + 8) == 0) return v;
            return static_cast<double>(d.search->created.time_since_epoch().count());
        };
        std::stable_sort(matched.begin(), matched.end(), [&](auto& a, auto& b)
        {
            return orderAscending ? sortKey(*a) < sortKey(*b) : sortKey(*a) > sortKey(*b);
        });

        std::vector<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails> results;
        for (auto& doc : matched) results.push_back(winrt::make<implementation::MultiplayerSearchHandleDetails>(doc));
        ::pdnet::Log(L"xsapi-pdnet: search %s: %zu session(s), %zu match '%s'", sessionTemplateName.c_str(), all.size(), results.size(), filter.c_str());
        co_return winrt::single_threaded_vector(std::move(results)).GetView();
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsyncImpl(Microsoft::Xbox::Services::Multiplayer::MultiplayerQuerySearchHandleRequest searchHandleRequest)
    {
        co_return co_await GetSearchHandlesAsync(searchHandleRequest.ServiceConfigurationId(), searchHandleRequest.SessionTemplateName(),
            searchHandleRequest.OrderBy(), searchHandleRequest.OrderAscending(), searchHandleRequest.SearchFilter());
    }
    // Public operations: results can be read more than once (see repeatable.h).
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        return ::pdnet::xsapi::Repeatable(WriteSessionAsyncImpl(multiplayerSession, multiplayerSessionWriteMode));
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode)
    {
        return ::pdnet::xsapi::Repeatable(TryWriteSessionAsyncImpl(multiplayerSession, multiplayerSessionWriteMode));
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::WriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        return ::pdnet::xsapi::Repeatable(WriteSessionByHandleAsyncImpl(multiplayerSession, multiplayerSessionWriteMode, handleId));
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::WriteSessionResult> MultiplayerService::TryWriteSessionByHandleAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSession multiplayerSession, Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionWriteMode multiplayerSessionWriteMode, hstring handleId)
    {
        return ::pdnet::xsapi::Repeatable(TryWriteSessionByHandleAsyncImpl(multiplayerSession, multiplayerSessionWriteMode, handleId));
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference sessionReference)
    {
        return ::pdnet::xsapi::Repeatable(GetCurrentSessionAsyncImpl(sessionReference));
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Multiplayer::MultiplayerSession> MultiplayerService::GetCurrentSessionByHandleAsync(hstring handleId)
    {
        return ::pdnet::xsapi::Repeatable(GetCurrentSessionByHandleAsyncImpl(handleId));
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsync(hstring serviceConfigurationId, hstring sessionTemplateName, hstring orderBy, bool orderAscending, hstring searchFilter)
    {
        return ::pdnet::xsapi::Repeatable(GetSearchHandlesAsyncImpl(serviceConfigurationId, sessionTemplateName, orderBy, orderAscending, searchFilter));
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSearchHandleDetails>> MultiplayerService::GetSearchHandlesAsync(Microsoft::Xbox::Services::Multiplayer::MultiplayerQuerySearchHandleRequest searchHandleRequest)
    {
        return ::pdnet::xsapi::Repeatable(GetSearchHandlesAsyncImpl(searchHandleRequest));
    }

    void MultiplayerService::EnableMultiplayerSubscriptions()
    {
        Register();
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
