#pragma once
#include "GameServerPlatform/GameServerMetadataResult.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct GameServerMetadataResult : GameServerMetadataResultT<GameServerMetadataResult>
    {
        GameServerMetadataResult() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameVariant> GameVariants();
        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::GameServerPlatform::GameServerImageSet> GameServerImageSets();
    };
}
