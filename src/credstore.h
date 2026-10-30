#pragma once
#include <windows.h>
#include <string>

namespace pdnet
{
    // The EOS SDK keeps its device ID in Windows Credential Manager (CredReadW/CredWriteW/CredDeleteW), which the
    // game's AppContainer sandbox may not use. This points those imports - in the EOS SDK module only - at a small
    // per-user store in `directory` (one DPAPI-protected file per credential). Returns the number of imports redirected.
    int RedirectCredentialStore(HMODULE eosModule, const std::wstring& directory);
}
