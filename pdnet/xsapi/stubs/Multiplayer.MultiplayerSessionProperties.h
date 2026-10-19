#pragma once
#include "Multiplayer/MultiplayerSessionProperties.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSessionProperties : MultiplayerSessionPropertiesT<MultiplayerSessionProperties>
    {
        MultiplayerSessionProperties() = default;

        Windows::Foundation::Collections::IVectorView<hstring> Keywords();
        void Keywords(Windows::Foundation::Collections::IVectorView<hstring> const& value);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction JoinRestriction();
        void JoinRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction const& value);
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction ReadRestriction();
        void ReadRestriction(Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction const& value);
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> TurnCollection();
        void TurnCollection(Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionMember> const& value);
        hstring MatchmakingTargetSessionConstantsJson();
        hstring SessionCustomPropertiesJson();
        hstring MatchmakingServerConnectionString();
        Windows::Foundation::Collections::IVectorView<hstring> ServerConnectionStringCandidates();
        Windows::Foundation::Collections::IVectorView<uint32_t> SessionOwnerIndices();
        hstring HostDeviceToken();
        bool Closed();
        bool AllocateCloudCompute();
    };
}
