#pragma once
#include "Multiplayer/MultiplayerSessionChangeEventArgs.g.h"

// pdnet-hand
namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionChangeEventArgs : MultiplayerSessionChangeEventArgsT<MultiplayerSessionChangeEventArgs>
    {
        MultiplayerSessionChangeEventArgs() = default;
        MultiplayerSessionChangeEventArgs(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference reference, hstring branch, uint64_t changeNumber)
            : m_reference(std::move(reference)), m_branch(std::move(branch)), m_changeNumber(changeNumber) {}

        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference() { return m_reference; }
        hstring Branch() { return m_branch; }
        uint64_t ChangeNumber() { return m_changeNumber; }

    private:
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference m_reference{ nullptr };
        hstring m_branch;
        uint64_t m_changeNumber = 0;
    };
}
