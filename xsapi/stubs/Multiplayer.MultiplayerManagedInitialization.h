#pragma once
#include "Multiplayer/MultiplayerManagedInitialization.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerManagedInitialization : MultiplayerManagedInitializationT<MultiplayerManagedInitialization>
    {
        MultiplayerManagedInitialization() = default;

        bool ManagedInitializationSet();
        Windows::Foundation::TimeSpan JoinTimeout();
        Windows::Foundation::TimeSpan MeasurementTimeout();
        Windows::Foundation::TimeSpan EvaluationTimeout();
        bool AutoEvaluate();
        uint32_t MembersNeededToStart();
    };
}
