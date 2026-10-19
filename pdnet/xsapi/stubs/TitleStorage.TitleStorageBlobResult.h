#pragma once
#include "TitleStorage/TitleStorageBlobResult.g.h"

// pdnet-hand
namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageBlobResult : TitleStorageBlobResultT<TitleStorageBlobResult>
    {
        TitleStorageBlobResult() = default;
        TitleStorageBlobResult(Windows::Storage::Streams::IBuffer buffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata metadata)
            : m_buffer(std::move(buffer)), m_metadata(std::move(metadata)) {}

        Windows::Storage::Streams::IBuffer BlobBuffer() { return m_buffer; }
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata BlobMetadata() { return m_metadata; }

    private:
        Windows::Storage::Streams::IBuffer m_buffer{ nullptr };
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata m_metadata{ nullptr };
    };
}
