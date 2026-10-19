#pragma once
#include "Tournaments/TeamInfo.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TeamInfo : TeamInfoT<TeamInfo>
    {
        TeamInfo() = default;

        hstring Id();
        hstring Name();
        Windows::Foundation::Collections::IVectorView<hstring> MemberXboxUserIds();
        Windows::Foundation::DateTime RegistrationDate();
        hstring Standing();
        Microsoft::Xbox::Services::Tournaments::TeamState TeamState();
        Microsoft::Xbox::Services::Tournaments::TeamCompletedReason CompletedReason();
        uint64_t Ranking();
        hstring ContinuationUri();
        Microsoft::Xbox::Services::Tournaments::CurrentMatchMetadata CurrentMatchMetadata();
        Microsoft::Xbox::Services::Tournaments::PreviousMatchMetadata PreviousMatchMetadata();
    };
}
