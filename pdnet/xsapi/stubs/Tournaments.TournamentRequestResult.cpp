#include "xsapi_pch.h"
#include "Tournaments.TournamentRequestResult.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::Tournament> TournamentRequestResult::Tournaments()
    {
        PDNET_XSAPI_STUB("TournamentRequestResult::Tournaments");
    }
    bool TournamentRequestResult::HasNext()
    {
        PDNET_XSAPI_STUB("TournamentRequestResult::HasNext");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TournamentRequestResult> TournamentRequestResult::GetNextAsync()
    {
        PDNET_XSAPI_STUB("TournamentRequestResult::GetNextAsync");
    }
}
