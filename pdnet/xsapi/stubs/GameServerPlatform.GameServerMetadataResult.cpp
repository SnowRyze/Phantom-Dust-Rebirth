#include "xsapi_pch.h"
#include "GameServerPlatform.GameServerMetadataResult.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameVariant> GameServerMetadataResult::GameVariants()
    {
        PDNET_XSAPI_STUB("GameServerMetadataResult::GameVariants");
    }
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerImageSet> GameServerMetadataResult::GameServerImageSets()
    {
        PDNET_XSAPI_STUB("GameServerMetadataResult::GameServerImageSets");
    }
}
