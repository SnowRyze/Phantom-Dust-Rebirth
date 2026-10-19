#include "xsapi_pch.h"
#include "GameServerPlatform.GameServerImageSet.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    uint64_t GameServerImageSet::MinPlayers()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::MinPlayers");
    }
    uint64_t GameServerImageSet::MaxPlayers()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::MaxPlayers");
    }
    hstring GameServerImageSet::Id()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::Id");
    }
    hstring GameServerImageSet::Name()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::Name");
    }
    uint64_t GameServerImageSet::SelectionOrder()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::SelectionOrder");
    }
    hstring GameServerImageSet::SchemaContent()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::SchemaContent");
    }
    hstring GameServerImageSet::SchemaName()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::SchemaName");
    }
    hstring GameServerImageSet::SchemaId()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::SchemaId");
    }
    Windows::Foundation::Collections::IMapView<hstring, hstring> GameServerImageSet::Tags()
    {
        PDNET_XSAPI_STUB("GameServerImageSet::Tags");
    }
}
