#pragma once
#include "TitleStorage/TitleStorageQuota.g.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageQuota : TitleStorageQuotaT<TitleStorageQuota>
    {
        TitleStorageQuota() = default;

        hstring ServiceConfigurationId();
        Microsoft::Xbox::Services::TitleStorage::TitleStorageType StorageType();
        hstring XboxUserId();
        hstring MultiplayerSessionTemplateName();
        hstring MultiplayerSessionName();
        uint64_t UsedBytes();
        uint64_t QuotaBytes();
    };
}
