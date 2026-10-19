#include "xsapi_pch.h"
#include "Leaderboard.LeaderboardService.h"


namespace winrt::Microsoft::Xbox::Services::Leaderboard::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName, uint32_t skipToRank, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, uint32_t skipToRank, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, uint32_t skipToRank, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, uint32_t skipToRank, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithAdditionalColumnsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring skipToXboxUserId, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithSkipToUserAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, hstring skipToXboxUserId, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithSkipToUserAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring skipToXboxUserId, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync(hstring serviceConfigurationId, hstring leaderboardName, hstring xuid, hstring socialGroup, hstring skipToXboxUserId, Windows::Foundation::Collections::IVectorView<hstring> additionalColumns, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardWithSkipToUserWithAdditionalColumnsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardForSocialGroupAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, hstring sortOrder, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardForSocialGroupAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupWithSkipToRankAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, uint32_t skipToRank, hstring sortOrder, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardForSocialGroupWithSkipToRankAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Leaderboard::LeaderboardResult> LeaderboardService::GetLeaderboardForSocialGroupWithSkipToUserAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName, hstring socialGroup, hstring skipToXboxUserId, hstring sortOrder, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("LeaderboardService::GetLeaderboardForSocialGroupWithSkipToUserAsync");
    }
}
