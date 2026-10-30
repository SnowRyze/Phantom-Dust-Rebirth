#pragma once
#include "XboxLiveContext.g.h"

// pdnet: holds the local user and hands out pdnet's service objects (created once per context).
namespace winrt::Microsoft::Xbox::Services::implementation
{
    struct XboxLiveContext : XboxLiveContextT<XboxLiveContext>
    {
        XboxLiveContext() = default;

        XboxLiveContext(Microsoft::Xbox::Services::System::XboxLiveUser const& user);
        Microsoft::Xbox::Services::System::XboxLiveUser User();
        Microsoft::Xbox::Services::Social::ProfileService ProfileService();
        Microsoft::Xbox::Services::Social::SocialService SocialService();
        Microsoft::Xbox::Services::Social::ReputationService ReputationService();
        Microsoft::Xbox::Services::Achievements::AchievementService AchievementService();
        Microsoft::Xbox::Services::Leaderboard::LeaderboardService LeaderboardService();
        Microsoft::Xbox::Services::UserStatistics::UserStatisticsService UserStatisticsService();
        Microsoft::Xbox::Services::Matchmaking::MatchmakingService MatchmakingService();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerService MultiplayerService();
        Microsoft::Xbox::Services::Tournaments::TournamentService TournamentService();
        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityService RealTimeActivityService();
        Microsoft::Xbox::Services::Presence::PresenceService PresenceService();
        Microsoft::Xbox::Services::GameServerPlatform::GameServerPlatformService GameServerPlatformService();
        Microsoft::Xbox::Services::TitleStorage::TitleStorageService TitleStorageService();
        Microsoft::Xbox::Services::Privacy::PrivacyService PrivacyService();
        Microsoft::Xbox::Services::System::StringService StringService();
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchService ContextualSearchService();
        Microsoft::Xbox::Services::Events::EventsService EventsService();
        Microsoft::Xbox::Services::XboxLiveContextSettings Settings();
        Microsoft::Xbox::Services::XboxLiveAppConfiguration AppConfig();

    private:
        // Creates each service on first use and keeps it, like XSAPI does.
        template <typename Projected, typename Impl>
        Projected Service(Projected& slot, const wchar_t* name)
        {
            std::lock_guard guard(m_lock);
            if (!slot)
            {
                slot = winrt::make<Impl>();
                ::pdnet::Log(L"xsapi-pdnet: XboxLiveContext.%s created", name);
            }
            return slot;
        }

        std::mutex m_lock;
        Microsoft::Xbox::Services::System::XboxLiveUser m_user{ nullptr };
        Microsoft::Xbox::Services::XboxLiveContextSettings m_settings{ nullptr };
        Microsoft::Xbox::Services::Social::ProfileService m_profile{ nullptr };
        Microsoft::Xbox::Services::Social::SocialService m_social{ nullptr };
        Microsoft::Xbox::Services::Social::ReputationService m_reputation{ nullptr };
        Microsoft::Xbox::Services::Achievements::AchievementService m_achievements{ nullptr };
        Microsoft::Xbox::Services::Leaderboard::LeaderboardService m_leaderboard{ nullptr };
        Microsoft::Xbox::Services::UserStatistics::UserStatisticsService m_userStatistics{ nullptr };
        Microsoft::Xbox::Services::Matchmaking::MatchmakingService m_matchmaking{ nullptr };
        Microsoft::Xbox::Services::Multiplayer::MultiplayerService m_multiplayer{ nullptr };
        Microsoft::Xbox::Services::Tournaments::TournamentService m_tournaments{ nullptr };
        Microsoft::Xbox::Services::RealTimeActivity::RealTimeActivityService m_rta{ nullptr };
        Microsoft::Xbox::Services::Presence::PresenceService m_presence{ nullptr };
        Microsoft::Xbox::Services::GameServerPlatform::GameServerPlatformService m_gameServer{ nullptr };
        Microsoft::Xbox::Services::TitleStorage::TitleStorageService m_titleStorage{ nullptr };
        Microsoft::Xbox::Services::Privacy::PrivacyService m_privacy{ nullptr };
        Microsoft::Xbox::Services::System::StringService m_strings{ nullptr };
        Microsoft::Xbox::Services::ContextualSearch::ContextualSearchService m_search{ nullptr };
        Microsoft::Xbox::Services::Events::EventsService m_events{ nullptr };
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct XboxLiveContext : XboxLiveContextT<XboxLiveContext, implementation::XboxLiveContext>
    {
    };
}
