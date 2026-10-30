#pragma once
#include "Multiplayer/WriteSessionResult.g.h"

// pdnet-hand
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct WriteSessionResult : WriteSessionResultT<WriteSessionResult>
    {
        WriteSessionResult() = default;
        WriteSessionResult(bool succeeded, Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus status, Microsoft::Xbox::Services::Multiplayer::MultiplayerSession session)
            : m_succeeded(succeeded), m_status(status), m_session(std::move(session)) {}

        bool Succeeded() { return m_succeeded; }
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession Session() { return m_session; }
        Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus Status() { return m_status; }

    private:
        bool m_succeeded = false;
        Microsoft::Xbox::Services::Multiplayer::WriteSessionStatus m_status{};
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSession m_session{ nullptr };
    };
}
