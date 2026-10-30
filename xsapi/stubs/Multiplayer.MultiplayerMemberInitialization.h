#pragma once
#include "Multiplayer/MultiplayerMemberInitialization.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerMemberInitialization : MultiplayerMemberInitializationT<MultiplayerMemberInitialization>
    {
        MultiplayerMemberInitialization() = default;

        bool MemberInitializationSet();
        Windows::Foundation::TimeSpan JoinTimeout();
        Windows::Foundation::TimeSpan MeasurementTimeout();
        Windows::Foundation::TimeSpan EvaluationTimeout();
        bool ExternalEvaluation();
        uint32_t MembersNeededToStart();
    };
}
