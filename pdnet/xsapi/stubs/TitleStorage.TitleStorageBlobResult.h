#pragma once
#include "TitleStorage/TitleStorageBlobResult.g.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageBlobResult : TitleStorageBlobResultT<TitleStorageBlobResult>
    {
        TitleStorageBlobResult() = default;

        Windows::Storage::Streams::IBuffer BlobBuffer();
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata BlobMetadata();
    };
}
