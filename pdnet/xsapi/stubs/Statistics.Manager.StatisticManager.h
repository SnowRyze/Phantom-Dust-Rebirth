#pragma once
#include "Statistics/Manager/StatisticManager.g.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    struct StatisticManager : StatisticManagerT<StatisticManager>
    {
        StatisticManager() = default;

        static Microsoft::Xbox::Services::Statistics::Manager::StatisticManager SingletonInstance();
        void AddLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void RemoveLocalUser(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void RequestFlushToService(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        void RequestFlushToService(Microsoft::Xbox::Services::System::XboxLiveUser const& user, bool isHighPriority);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Statistics::Manager::StatisticEvent> DoWork();
        void SetStatisticNumberData(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, double value);
        void SetStatisticIntegerData(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, int64_t value);
        void SetStatisticStringData(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name, hstring const& value);
        Windows::Foundation::Collections::IVectorView<hstring> GetStatisticNames(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        Microsoft::Xbox::Services::Statistics::Manager::StatisticValue GetStatistic(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name);
        void DeleteStatistic(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& name);
        void GetLeaderboard(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& statName, Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery const& query);
        void GetSocialLeaderboard(Microsoft::Xbox::Services::System::XboxLiveUser const& user, hstring const& statName, hstring const& socialGroup, Microsoft::Xbox::Services::Leaderboard::LeaderboardQuery const& query);
    };
}
namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::factory_implementation
{
    struct StatisticManager : StatisticManagerT<StatisticManager, implementation::StatisticManager>
    {
    };
}
