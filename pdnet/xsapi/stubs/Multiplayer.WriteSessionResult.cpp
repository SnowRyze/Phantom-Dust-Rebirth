#include "xsapi_pch.h"
#include "Multiplayer.WriteSessionResult.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    bool WriteSessionResult::Succeeded()
    {
        PDNET_XSAPI_STUB("WriteSessionResult::Succeeded");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSession WriteSessionResult::Session()
    {
        PDNET_XSAPI_STUB("WriteSessionResult::Session");
    }
    Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus WriteSessionResult::Status()
    {
        PDNET_XSAPI_STUB("WriteSessionResult::Status");
    }
}
