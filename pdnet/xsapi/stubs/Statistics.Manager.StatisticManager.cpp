#include "xsapi_pch.h"
#include "Statistics.Manager.StatisticManager.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    Microsoft::Xbox::Services::Statistics::Manager::StatisticManager StatisticManager::SingletonInstance()
    {
        PDNET_XSAPI_STUB("StatisticManager::SingletonInstance");
    }
    void StatisticManager::AddLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("StatisticManager::AddLocalUser");
    }
    void StatisticManager::RemoveLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("StatisticManager::RemoveLocalUser");
    }
    void StatisticManager::RequestFlushToService(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("StatisticManager::RequestFlushToService");
    }
    void StatisticManager::RequestFlushToService(Microsoft::Xbox::Services::System::XboxLiveUser const& user, bool isHighPriority)
    {
        PDNET_XSAPI_STUB("StatisticManager::RequestFlushToService");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Statistics::Manager::StatisticEvent> StatisticManager::DoWork()
    {
        PDNET_XSAPI_STUB("StatisticManager::DoWork");
    }
    void StatisticManager::SetStatisticNumberData(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, double value)
    {
        PDNET_XSAPI_STUB("StatisticManager::SetStatisticNumberData");
    }
    void StatisticManager::SetStatisticIntegerData(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, int64_t value)
    {
        PDNET_XSAPI_STUB("StatisticManager::SetStatisticIntegerData");
    }
    void StatisticManager::SetStatisticStringData(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, hstring const& value)
    {
        PDNET_XSAPI_STUB("StatisticManager::SetStatisticStringData");
    }
    Windows::Foundation::Collections::IVectorView<hstring> StatisticManager::GetStatisticNames(Microsoft::Xbox::Services::System::XboxLiveUser const& user)
    {
        PDNET_XSAPI_STUB("StatisticManager::GetStatisticNames");
    }
    Microsoft::Xbox::Services::Statistics::Manager::StatisticValue StatisticManager::GetStatistic(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name)
    {
        PDNET_XSAPI_STUB("StatisticManager::GetStatistic");
    }
    void StatisticManager::DeleteStatistic(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name)
    {
        PDNET_XSAPI_STUB("StatisticManager::DeleteStatistic");
    }
    void StatisticManager::GetLeaderboard(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& statName, Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery const& query)
    {
        PDNET_XSAPI_STUB("StatisticManager::GetLeaderboard");
    }
    void StatisticManager::GetSocialLeaderboard(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& statName, hstring const& socialGroup, Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery const& query)
    {
        PDNET_XSAPI_STUB("StatisticManager::GetSocialLeaderboard");
    }
}
