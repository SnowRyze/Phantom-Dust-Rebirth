#include "xsapi_pch.h"
#include "Multiplayer.Manager.FindMatchCompletedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Microsoft::Xbox::Services::Multiplayer::Manager::MatchStatus FindMatchCompletedEventArgs::MatchStatus()
    {
        PDNET_XSAPI_STUB("FindMatchCompletedEventArgs::MatchStatus");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerMeasurementFailure FindMatchCompletedEventArgs::InitializationFailureCause()
    {
        PDNET_XSAPI_STUB("FindMatchCompletedEventArgs::InitializationFailureCause");
    }
}
