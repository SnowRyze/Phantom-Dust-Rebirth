#pragma once
#include "TitleStorage/TitleStorageBlobMetadata.g.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageBlobMetadata : TitleStorageBlobMetadataT<TitleStorageBlobMetadata>
    {
        TitleStorageBlobMetadata() = default;

        TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId);
        TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId, hstring const& displayName, hstring const& eTag);
        TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId, hstring const& displayName, hstring const& eTag, Windows::Foundation::DateTime const& clientTimestamp);
        static Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata CreateTitleStorageBlobMetadataForSessionStorage(hstring const& serviceConfigurationId, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& multiplayerSessionTemplateName, hstring const& multiplayerSessionName, hstring const& displayName, hstring const& eTag);
        hstring BlobPath();
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType BlobType();
        Microsoft::Xbox::Services::TitleStorage::TitleStorageType StorageType();
        hstring DisplayName();
        hstring ETag();
        Windows::Foundation::DateTime ClientTimestamp();
        void set(Windows::Foundation::DateTime const& value);
        uint64_t Length();
        hstring ServiceConfigurationId();
        hstring XboxUserId();
        hstring MultiplayerSessionTemplateName();
        hstring MultiplayerSessionName();
    };
}
namespace winrt::Microsoft::Xbox::Services::TitleStorage::factory_implementation
{
    struct TitleStorageBlobMetadata : TitleStorageBlobMetadataT<TitleStorageBlobMetadata, implementation::TitleStorageBlobMetadata>
    {
    };
}
