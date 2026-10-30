#pragma once
#include "Multiplayer/MultiplayerSessionStates.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionStates : MultiplayerSessionStatesT<MultiplayerSessionStates>
    {
        MultiplayerSessionStates() = default;

        Windows::Foundation::DateTime StartTime();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionStatus Status();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility Visibility();
        bool IsMyTurn();
        hstring XboxUserId();
        uint32_t AcceptedMemberCount();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction JoinRestriction();
        Windows::Foundation::Collections::IVectorView<hstring> Keywords();
    };
}
