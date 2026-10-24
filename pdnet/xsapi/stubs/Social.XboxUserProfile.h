#pragma once
#include "Social/XboxUserProfile.g.h"

// pdnet-hand: a player's profile: id and name (xsapi::NameOf), nothing else.
namespace winrt::Microsoft::Xbox::Services::Social::implementation
{
    struct XboxUserProfile : XboxUserProfileT<XboxUserProfile>
    {
        XboxUserProfile() = default;
        XboxUserProfile(hstring xuid, hstring name) : m_xuid(std::move(xuid)), m_name(std::move(name)) {}

        hstring ApplicationDisplayName() { return m_name; }
        Windows::Foundation::Uri ApplicationDisplayPictureResizeUri() { return nullptr; }
        hstring GameDisplayName() { return m_name; }
        Windows::Foundation::Uri GameDisplayPictureResizeUri() { return nullptr; }
        hstring Gamerscore() { return L"0"; }
        hstring Gamertag() { return m_name; }
        hstring XboxUserId() { return m_xuid; }

    private:
        hstring m_xuid, m_name;
    };
}
