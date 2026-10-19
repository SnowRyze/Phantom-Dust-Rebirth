#include "xsapi_pch.h"
#include "GameServerPlatform.GameVariant.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    hstring GameVariant::Id()
    {
        PDNET_XSAPI_STUB("GameVariant::Id");
    }
    hstring GameVariant::Name()
    {
        PDNET_XSAPI_STUB("GameVariant::Name");
    }
    bool GameVariant::IsPublisher()
    {
        PDNET_XSAPI_STUB("GameVariant::IsPublisher");
    }
    uint64_t GameVariant::Rank()
    {
        PDNET_XSAPI_STUB("GameVariant::Rank");
    }
    hstring GameVariant::SchemaContent()
    {
        PDNET_XSAPI_STUB("GameVariant::SchemaContent");
    }
    hstring GameVariant::SchemaName()
    {
        PDNET_XSAPI_STUB("GameVariant::SchemaName");
    }
    hstring GameVariant::SchemaId()
    {
        PDNET_XSAPI_STUB("GameVariant::SchemaId");
    }
}
