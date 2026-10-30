#pragma once
#include "Multiplayer/MultiplayerSessionReference.g.h"

// pdnet-hand
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionReference : MultiplayerSessionReferenceT<MultiplayerSessionReference>
    {
        MultiplayerSessionReference() = default;
        MultiplayerSessionReference(hstring const& serviceConfigurationId, hstring const& sessionTemplateName, hstring const& sessionName)
            : m_scid(serviceConfigurationId), m_template(sessionTemplateName), m_name(sessionName) {}

        // "/serviceconfigs/{scid}/sessionTemplates/{template}/sessions/{name}"
        static Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference ParseFromUriPath(hstring const& uriPath)
        {
            std::wstring p{ uriPath };
            auto part = [&](const wchar_t* key) -> std::wstring
            {
                auto at = p.find(key);
                if (at == std::wstring::npos) return {};
                at += wcslen(key);
                auto end = p.find(L'/', at);
                return p.substr(at, end == std::wstring::npos ? std::wstring::npos : end - at);
            };
            return winrt::make<MultiplayerSessionReference>(hstring{ part(L"/serviceconfigs/") }, hstring{ part(L"/sessionTemplates/") }, hstring{ part(L"/sessions/") });
        }

        hstring ServiceConfigurationId() { return m_scid; }
        hstring SessionTemplateName() { return m_template; }
        hstring SessionName() { return m_name; }
        hstring ToUriPath() { return L"/serviceconfigs/" + m_scid + L"/sessionTemplates/" + m_template + L"/sessions/" + m_name; }

    private:
        hstring m_scid, m_template, m_name;
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerSessionReference : MultiplayerSessionReferenceT<MultiplayerSessionReference, implementation::MultiplayerSessionReference>
    {
    };
}
