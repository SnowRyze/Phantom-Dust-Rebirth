#include "xsapi_pch.h"
#include "Tournaments.TeamRequestResult.h"


namespace winrt::Microsoft::Xbox::Services::Tournaments::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Tournaments::TeamInfo> TeamRequestResult::Teams()
    {
        PDNET_XSAPI_STUB("TeamRequestResult::Teams");
    }
    bool TeamRequestResult::HasNext()
    {
        PDNET_XSAPI_STUB("TeamRequestResult::HasNext");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::Tournaments::TeamRequestResult> TeamRequestResult::GetNextAsync()
    {
        PDNET_XSAPI_STUB("TeamRequestResult::GetNextAsync");
    }
}
