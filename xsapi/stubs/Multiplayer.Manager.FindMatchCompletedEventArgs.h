#pragma once
#include "Multiplayer/Manager/FindMatchCompletedEventArgs.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    struct FindMatchCompletedEventArgs : FindMatchCompletedEventArgsT<FindMatchCompletedEventArgs>
    {
        FindMatchCompletedEventArgs() = default;

        Microsoft::Xbox::Services::Multiplayer::Manager::MatchStatus MatchStatus();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerMeasurementFailure InitializationFailureCause();
    };
}
