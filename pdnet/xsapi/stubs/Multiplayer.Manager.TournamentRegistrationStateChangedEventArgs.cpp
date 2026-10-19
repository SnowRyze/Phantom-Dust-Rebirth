#include "xsapi_pch.h"
#include "Multiplayer.Manager.TournamentRegistrationStateChangedEventArgs.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::Manager::implementation
{
    Microsoft::Xbox::Services::Tournaments::TournamentRegistrationState TournamentRegistrationStateChangedEventArgs::RegistrationState()
    {
        PDNET_XSAPI_STUB("TournamentRegistrationStateChangedEventArgs::RegistrationState");
    }
    Microsoft::Xbox::Services::Tournaments::TournamentRegistrationReason TournamentRegistrationStateChangedEventArgs::RegistrationReason()
    {
        PDNET_XSAPI_STUB("TournamentRegistrationStateChangedEventArgs::RegistrationReason");
    }
}
