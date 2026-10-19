#pragma once
#include "Tournaments/TeamRequestResult.g.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    struct TeamRequestResult : TeamRequestResultT<TeamRequestResult>
    {
        TeamRequestResult() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TeamInfo> Teams();
        bool HasNext();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TeamRequestResult> GetNextAsync();
    };
}
