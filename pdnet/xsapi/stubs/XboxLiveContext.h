#pragma once
#include "XboxLiveContext.g.h"


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
    };
}
namespace winrt::Microsoft::Xbox::Services::factory_implementation
{
    struct XboxLiveContext : XboxLiveContextT<XboxLiveContext, implementation::XboxLiveContext>
    {
    };
}
