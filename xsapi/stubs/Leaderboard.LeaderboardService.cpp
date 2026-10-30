// pdnet-hand: every query reads the global EOS leaderboard of the same name (src/scores.h); social groups (friends) get it too.
#include "xsapi_pch.h"
#include "Leaderboard.LeaderboardService.h"
#include "Leaderboard.LeaderboardResult.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardAsync", leaderboardName, hstring{}, 0, 0);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName, uint32_t skipToRank, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardAsync", leaderboardName, hstring{}, skipToRank, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardAsync", leaderboardName, hstring{}, 0, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, uint32_t skipToRank, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardAsync", leaderboardName, hstring{}, skipToRank, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithAdditionalColumnsAsync", leaderboardName, hstring{}, 0, 0, additionalColumns);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithAdditionalColumnsAsync", leaderboardName, hstring{}, 0, maxItems, additionalColumns);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, uint32_t skipToRank, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithAdditionalColumnsAsync", leaderboardName, hstring{}, skipToRank, maxItems, additionalColumns);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, uint32_t skipToRank, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithAdditionalColumnsAsync", leaderboardName, hstring{}, skipToRank, maxItems, additionalColumns);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring skipToXboxUserId, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithSkipToUserAsync", leaderboardName, skipToXboxUserId, 0, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, hstring skipToXboxUserId, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithSkipToUserAsync", leaderboardName, skipToXboxUserId, 0, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring skipToXboxUserId, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync", leaderboardName, skipToXboxUserId, 0, maxItems, additionalColumns);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, hstring skipToXboxUserId, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync", leaderboardName, skipToXboxUserId, 0, maxItems, additionalColumns);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardForSocialGroupAsync", statisticName, hstring{}, 0, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, hstring sortOrder, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardForSocialGroupAsync", statisticName, hstring{}, 0, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupWithSkipToRankAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, uint32_t skipToRank, hstring sortOrder, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardForSocialGroupWithSkipToRankAsync", statisticName, hstring{}, skipToRank, maxItems);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupWithSkipToUserAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, hstring skipToXboxUserId, hstring sortOrder, uint32_t maxItems)
    {
        co_await winrt::resume_background();
        co_return LeaderboardResult::Page(L"GetLeaderboardForSocialGroupWithSkipToUserAsync", statisticName, skipToXboxUserId, 0, maxItems);
    }
}
