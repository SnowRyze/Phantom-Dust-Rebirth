#pragma once
#include <functional>
#include <string>

// Long-lived Epic Online Services connection (pdnet.ini: eos=on). One background thread owns the EOS platform:
// it signs in with this PC's device ID (no Epic account), keeps ticking EOS, signs in again when the session
// expires or drops, and runs posted work. Later steps (lobbies, P2P) build on it.
namespace pdnet::eos
{
    // Starts the EOS thread once. No-op if the SDK or eos.ini is missing (logged).
    void Start();

    bool SignedIn();
    std::wstring LocalUserId();   // product user id, empty until signed in
    std::wstring DisplayName();   // from pdnet.ini display_name

    // Runs `work` on the EOS thread, where every EOS call must happen. `platform` is the EOS_HPlatform.
    void Post(std::function<void(void* platform)> work);

    // Called on the EOS thread when the sign-in is lost (false) and when it comes back afterwards (true).
    void SetSignInListener(std::function<void(bool signedIn)> listener);
}
