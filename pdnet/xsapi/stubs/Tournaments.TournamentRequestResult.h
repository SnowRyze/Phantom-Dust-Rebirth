#pragma once
#include "Tournaments/TournamentRequestResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TournamentRequestResult : TournamentRequestResultT<TournamentRequestResult>
    {
        TournamentRequestResult() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::Tournament> Tournaments();
        bool HasNext();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TournamentRequestResult> GetNextAsync();
    };
}
