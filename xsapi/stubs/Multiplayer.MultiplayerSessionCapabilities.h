#pragma once
#include "Multiplayer/MultiplayerSessionCapabilities.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionCapabilities : MultiplayerSessionCapabilitiesT<MultiplayerSessionCapabilities>
    {
        MultiplayerSessionCapabilities() = default;

        bool Connectivity();
        void Connectivity(bool value);
        bool SuppressPresenceActivityCheck();
        void SuppressPresenceActivityCheck(bool value);
        bool Gameplay();
        void Gameplay(bool value);
        bool Large();
        void Large(bool value);
        bool ConnectionRequiredForActiveMembers();
        void ConnectionRequiredForActiveMembers(bool value);
        bool UserAuthorizationStyle();
        void UserAuthorizationStyle(bool value);
        bool Crossplay();
        void Crossplay(bool value);
        bool Team();
        void Team(bool value);
        bool Arbitration();
        void Arbitration(bool value);
        bool Searchable();
        void Searchable(bool value);
        bool HasOwners();
        void HasOwners(bool value);
    };
}
namespace winrt::Microsoft::Xbox::Services::Multiplayer::factory_implementation
{
    struct MultiplayerSessionCapabilities : MultiplayerSessionCapabilitiesT<MultiplayerSessionCapabilities, implementation::MultiplayerSessionCapabilities>
    {
    };
}
