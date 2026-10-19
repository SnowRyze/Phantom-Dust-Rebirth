#include "xsapi_pch.h"
#include "Statistics.Manager.LeaderboardResultEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Statistics::Manager::implementation
{
    Microsoft::Xbox::Services::Leaderboard::LeaderboardResult LeaderboardResultEventArgs::Result()
    {
        PDNET_XSAPI_STUB("LeaderboardResultEventArgs::Result");
    }
}
