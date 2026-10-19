#include "xsapi_pch.h"
#include "Social.Manager.TitleHistory.h"


namespace winrt::Microsoft::Xbox::Services::Social::Manager::implementation
{
    bool TitleHistory::HasUserPlayed()
    {
        PDNET_XSAPI_STUB("TitleHistory::HasUserPlayed");
    }
    Windows::Foundation::DateTime TitleHistory::LastTimeUserPlayed()
    {
        PDNET_XSAPI_STUB("TitleHistory::LastTimeUserPlayed");
    }
}
