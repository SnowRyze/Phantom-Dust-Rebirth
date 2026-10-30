#pragma once
#include "System/GetTokenAndSignatureResult.g.h"

// pdnet: always an empty token/signature (there is no Xbox Live), with the local player's details.
namespace winrt::Microsoft::Xbox::Services::System::implementation
{
    struct GetTokenAndSignatureResult : GetTokenAndSignatureResultT<GetTokenAndSignatureResult>
    {
        GetTokenAndSignatureResult() = default;
        GetTokenAndSignatureResult(hstring xuid, hstring gamertag, hstring ageGroup, hstring privileges, hstring webAccountId)
            : m_xuid(std::move(xuid)), m_gamertag(std::move(gamertag)), m_ageGroup(std::move(ageGroup)),
              m_privileges(std::move(privileges)), m_webAccountId(std::move(webAccountId)) {}

        hstring Token() { return {}; }
        hstring Signature() { return {}; }
        hstring XboxUserId() { return m_xuid; }
        hstring Gamertag() { return m_gamertag; }
        hstring XboxUserHash() { return {}; }
        hstring AgeGroup() { return m_ageGroup; }
        hstring Privileges() { return m_privileges; }
        hstring WebAccountId() { return m_webAccountId; }
        hstring Reserved() { return {}; }

    private:
        hstring m_xuid, m_gamertag, m_ageGroup, m_privileges, m_webAccountId;
    };
}
