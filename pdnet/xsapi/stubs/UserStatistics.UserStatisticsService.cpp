#include "xsapi_pch.h"
#include "UserStatistics.UserStatisticsService.h"


namespace winrt::Microsoft::Xbox::Services::UserStatistics::implementation
{
    winrt::event_token UserStatisticsService::StatisticChanged(Windows::Foundation::EventHandler<Microsoft::Xbox::Services::UserStatistics::StatisticChangeEventArgs> const& __param0)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::StatisticChanged");
    }
    void UserStatisticsService::StatisticChanged(winrt::event_token const& __param0) noexcept
    {
        PDNET_XSAPI_STUB_NOEXCEPT("UserStatisticsService::StatisticChanged");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult> UserStatisticsService::GetSingleUserStatisticsAsync(hstring xboxUserId, hstring serviceConfigurationId, hstring statisticName)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::GetSingleUserStatisticsAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult> UserStatisticsService::GetSingleUserStatisticsAsync(hstring xboxUserId, hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> statisticNames)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::GetSingleUserStatisticsAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>> UserStatisticsService::GetMultipleUserStatisticsAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, hstring serviceConfigurationId, Windows::Foundation::Collections::IVectorView<hstring> statisticNames)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::GetMultipleUserStatisticsAsync");
    }
    Windows::Foundation::IAsyncOperation<Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::UserStatisticsResult>> UserStatisticsService::GetMultipleUserStatisticsForMultipleServiceConfigurationsAsync(Windows::Foundation::Collections::IVectorView<hstring> xboxUserIds, Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::UserStatistics::RequestedStatistics> requestedServiceConfigurationStatisticsCollection)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::GetMultipleUserStatisticsForMultipleServiceConfigurationsAsync");
    }
    Microsoft::Xbox::Services::UserStatistics::StatisticChangeSubscription UserStatisticsService::SubscribeToStatisticChange(hstring const& xboxUserId, hstring const& serviceConfigurationId, hstring const& statisticName)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::SubscribeToStatisticChange");
    }
    void UserStatisticsService::UnsubscribeFromStatisticChange(Microsoft::Xbox::Services::UserStatistics::StatisticChangeSubscription const& subscription)
    {
        PDNET_XSAPI_STUB("UserStatisticsService::UnsubscribeFromStatisticChange");
    }
}
