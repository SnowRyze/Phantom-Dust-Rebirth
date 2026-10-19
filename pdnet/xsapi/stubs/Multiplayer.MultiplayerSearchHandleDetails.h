#pragma once
#include "Multiplayer/MultiplayerSearchHandleDetails.g.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    struct MultiplayerSearchHandleDetails : MultiplayerSearchHandleDetailsT<MultiplayerSearchHandleDetails>
    {
        MultiplayerSearchHandleDetails() = default;

        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference SessionReference();
        hstring HandleId();
        Windows::Foundation::Collections::IVectorView<hstring> SessionOwnerXboxUserIds();
        Windows::Foundation::Collections::IVectorView<hstring> Tags();
        Windows::Foundation::Collections::IMapView<hstring, double> NumbersMetadata();
        Windows::Foundation::Collections::IMapView<hstring, hstring> StringsMetadata();
        Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> RoleTypes();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility Visibility();
        Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction JoinRestriction();
        bool Closed();
        uint32_t MaxMembersCount();
        uint32_t MembersCount();
        Windows::Foundation::DateTime HandleCreationTime();
        hstring CustomSessionPropertiesJson();
    };
}
