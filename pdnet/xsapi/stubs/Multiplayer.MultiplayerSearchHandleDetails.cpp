#include "xsapi_pch.h"
#include "Multiplayer.MultiplayerSearchHandleDetails.h"


namespace winrt::Microsoft::Xbox::Services::Multiplayer::implementation
{
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionReference MultiplayerSearchHandleDetails::SessionReference()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::SessionReference");
    }
    hstring MultiplayerSearchHandleDetails::HandleId()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::HandleId");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSearchHandleDetails::SessionOwnerXboxUserIds()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::SessionOwnerXboxUserIds");
    }
    Windows::Foundation::Collections::IVectorView<hstring> MultiplayerSearchHandleDetails::Tags()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::Tags");
    }
    Windows::Foundation::Collections::IMapView<hstring, double> MultiplayerSearchHandleDetails::NumbersMetadata()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::NumbersMetadata");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> MultiplayerSearchHandleDetails::StringsMetadata()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::StringsMetadata");
    }
    Windows::Foundation::Collections::IMapView<hstring, Microsoft::Xbox::Services::Multiplayer::MultiplayerRoleType> MultiplayerSearchHandleDetails::RoleTypes()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::RoleTypes");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionVisibility MultiplayerSearchHandleDetails::Visibility()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::Visibility");
    }
    Microsoft::Xbox::Services::Multiplayer::MultiplayerSessionRestriction MultiplayerSearchHandleDetails::JoinRestriction()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::JoinRestriction");
    }
    bool MultiplayerSearchHandleDetails::Closed()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::Closed");
    }
    uint32_t MultiplayerSearchHandleDetails::MaxMembersCount()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::MaxMembersCount");
    }
    uint32_t MultiplayerSearchHandleDetails::MembersCount()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::MembersCount");
    }
    Windows::Foundation::DateTime MultiplayerSearchHandleDetails::HandleCreationTime()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::HandleCreationTime");
    }
    hstring MultiplayerSearchHandleDetails::CustomSessionPropertiesJson()
    {
        PDNET_XSAPI_STUB("MultiplayerSearchHandleDetails::CustomSessionPropertiesJson");
    }
}
