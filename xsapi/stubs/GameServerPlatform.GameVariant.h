#pragma once
#include "GameServerPlatform/GameVariant.g.h"


namespace winrt::Microsoft::Xbox::Services::GameServerPlatform::implementation
{
    struct GameVariant : GameVariantT<GameVariant>
    {
        GameVariant() = default;

        hstring Id();
        hstring Name();
        bool IsPublisher();
        uint64_t Rank();
        hstring SchemaContent();
        hstring SchemaName();
        hstring SchemaId();
    };
}
