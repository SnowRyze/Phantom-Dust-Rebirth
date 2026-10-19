#pragma once
#include "Statistics/Manager/LeaderboardResultEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    struct LeaderboardResultEventArgs : LeaderboardResultEventArgsT<LeaderboardResultEventArgs>
    {
        LeaderboardResultEventArgs() = default;

        Microsoft::Xbox::Services::Leaderboard::LeaderboardResult Result();
    };
}
