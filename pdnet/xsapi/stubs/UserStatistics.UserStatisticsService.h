#pragma once
#include "UserStatistics/UserStatisticsService.g.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    struct UserStatisticsService : UserStatisticsServiceT<UserStatisticsService>
    {
        UserStatisticsService() = default;

        winrt::event_token StatisticChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::UserStatistics::StatisticChangeEventArgs> const& __param0);
        void StatisticChanged(winrt::event_token const& __param0) noexcept;
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult> GetSingleUserStatisticsAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult> GetSingleUserStatisticsAsync(hstring xboxUserId, hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> statisticNames);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>> GetMultipleUserStatisticsAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> statisticNames);
        Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>> GetMultipleUserStatisticsForMultipleServiceConfigurationsAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::RequestedStatistics> requestedServiceConfigurationStatisticsCollection);
        Microsoft::Xbox::Services::UserStatistics::StatisticChangeSubscription SubscribeToStatisticChange(hstring const& xboxUserId, hstring const& serviceConfigurationId, hstring const& statisticName);
        void UnsubscribeFromStatisticChange(Microsoft::Xbox::Services::UserStatistics::StatisticChangeSubscription const& subscription);
    };
}
