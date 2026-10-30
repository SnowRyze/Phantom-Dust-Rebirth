// pdnet-hand
#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSessionConstants.h"
#include "Multiplayer.MultiplayerManagedInitialization.h"
#include "Multiplayer.MultiplayerMemberInitialization.h"
#include "Multiplayer.MultiplayerPeerToPeerRequirements.h"
#include "Multiplayer.MultiplayerPeerToHostRequirements.h"

namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MultiplayerManagedInitialization MultiplayerSessionConstants::ManagedInitialization() { return winrt::make<implementation::MultiplayerManagedInitialization>(); }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerMemberInitialization MultiplayerSessionConstants::MemberInitialization() { return winrt::make<implementation::MultiplayerMemberInitialization>(); }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerPeerToPeerRequirements MultiplayerSessionConstants::PeerToPeerRequirements() { return winrt::make<implementation::MultiplayerPeerToPeerRequirements>(); }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerPeerToHostRequirements MultiplayerSessionConstants::PeerToHostRequirements() { return winrt::make<implementation::MultiplayerPeerToHostRequirements>(); }
}
