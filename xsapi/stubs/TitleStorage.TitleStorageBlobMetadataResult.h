#pragma once
#include "TitleStorage/TitleStorageBlobMetadataResult.g.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageBlobMetadataResult : TitleStorageBlobMetadataResultT<TitleStorageBlobMetadataResult>
    {
        TitleStorageBlobMetadataResult() = default;

        Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata> Items();
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> GetNextAsync(uint32_t maxItems);
        bool HasNext();
    };
}
