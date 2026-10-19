#include "xsapi_pch.h"
#include "XboxLiveContext.h"
#include "XboxLiveContextSettings.h"
#include "XboxLiveAppConfiguration.h"
#include "Social.ProfileService.h"
#include "Social.SocialService.h"
#include "Social.ReputationService.h"
#include "Achievements.AchievementService.h"
#include "Leaderboard.LeaderboardService.h"
#include "UserStatistics.UserStatisticsService.h"
#include "Matchmaking.MatchmakingService.h"
#include "Multiplayer.MultiplayerService.h"
#include "Tournaments.TournamentService.h"
#include "RealTimeActivity.RealTimeActivityService.h"
#include "Presence.PresenceService.h"
#include "GameServerPlatform.GameServerPlatformService.h"
#include "TitleStorage.TitleStorageService.h"
#include "Privacy.PrivacyService.h"
#include "System.StringService.h"
#include "ContextualSearch.ContextualSearchService.h"
#include "Events.EventsService.h"

namespace winrt::Microsoft::Xbox::Services::implementation
{
    namespace xs = winrt::Microsoft::Xbox::Services;

    XboxLiveContext::XboxLiveContext(xs::System::XboxLiveUser const& user) : m_user(user)
    {
        m_settings = winrt::make<implementation::XboxLiveContextSettings>();
        ::pdnet::Log(L"xsapi-pdnet: XboxLiveContext created for '%s'", user ? user.Gamertag().c_str() : L"(no user)");
    }

    xs::System::XboxLiveUser XboxLiveContext::User() { return m_user; }
    xs::XboxLiveContextSettings XboxLiveContext::Settings() { return m_settings; }
    xs::XboxLiveAppConfiguration XboxLiveContext::AppConfig() { return implementation::XboxLiveAppConfiguration::SingletonInstance(); }

    xs::Social::ProfileService XboxLiveContext::ProfileService() { return Service<xs::Social::ProfileService, xs::Social::implementation::ProfileService>(m_profile, L"ProfileService"); }
    xs::Social::SocialService XboxLiveContext::SocialService() { return Service<xs::Social::SocialService, xs::Social::implementation::SocialService>(m_social, L"SocialService"); }
    xs::Social::ReputationService XboxLiveContext::ReputationService() { return Service<xs::Social::ReputationService, xs::Social::implementation::ReputationService>(m_reputation, L"ReputationService"); }
    xs::Achievements::AchievementService XboxLiveContext::AchievementService() { return Service<xs::Achievements::AchievementService, xs::Achievements::implementation::AchievementService>(m_achievements, L"AchievementService"); }
    xs::Leaderboard::LeaderboardService XboxLiveContext::LeaderboardService() { return Service<xs::Leaderboard::LeaderboardService, xs::Leaderboard::implementation::LeaderboardService>(m_leaderboard, L"LeaderboardService"); }
    xs::UserStatistics::UserStatisticsService XboxLiveContext::UserStatisticsService() { return Service<xs::UserStatistics::UserStatisticsService, xs::UserStatistics::implementation::UserStatisticsService>(m_userStatistics, L"UserStatisticsService"); }
    xs::Matchmaking::MatchmakingService XboxLiveContext::MatchmakingService() { return Service<xs::Matchmaking::MatchmakingService, xs::Matchmaking::implementation::MatchmakingService>(m_matchmaking, L"MatchmakingService"); }
    xs::Multiplayer::MultiplayerService XboxLiveContext::MultiplayerService() { return Service<xs::Multiplayer::MultiplayerService, xs::Multiplayer::implementation::MultiplayerService>(m_multiplayer, L"MultiplayerService"); }
    xs::Tournaments::TournamentService XboxLiveContext::TournamentService() { return Service<xs::Tournaments::TournamentService, xs::Tournaments::implementation::TournamentService>(m_tournaments, L"TournamentService"); }
    xs::RealTimeActivity::RealTimeActivityService XboxLiveContext::RealTimeActivityService() { return Service<xs::RealTimeActivity::RealTimeActivityService, xs::RealTimeActivity::implementation::RealTimeActivityService>(m_rta, L"RealTimeActivityService"); }
    xs::Presence::PresenceService XboxLiveContext::PresenceService() { return Service<xs::Presence::PresenceService, xs::Presence::implementation::PresenceService>(m_presence, L"PresenceService"); }
    xs::GameServerPlatform::GameServerPlatformService XboxLiveContext::GameServerPlatformService() { return Service<xs::GameServerPlatform::GameServerPlatformService, xs::GameServerPlatform::implementation::GameServerPlatformService>(m_gameServer, L"GameServerPlatformService"); }
    xs::TitleStorage::TitleStorageService XboxLiveContext::TitleStorageService() { return Service<xs::TitleStorage::TitleStorageService, xs::TitleStorage::implementation::TitleStorageService>(m_titleStorage, L"TitleStorageService"); }
    xs::Privacy::PrivacyService XboxLiveContext::PrivacyService() { return Service<xs::Privacy::PrivacyService, xs::Privacy::implementation::PrivacyService>(m_privacy, L"PrivacyService"); }
    xs::System::StringService XboxLiveContext::StringService() { return Service<xs::System::StringService, xs::System::implementation::StringService>(m_strings, L"StringService"); }
    xs::ContextualSearch::ContextualSearchService XboxLiveContext::ContextualSearchService() { return Service<xs::ContextualSearch::ContextualSearchService, xs::ContextualSearch::implementation::ContextualSearchService>(m_search, L"ContextualSearchService"); }
    xs::Events::EventsService XboxLiveContext::EventsService() { return Service<xs::Events::EventsService, xs::Events::implementation::EventsService>(m_events, L"EventsService"); }
}
