#pragma once
#include "TitleStorage/TitleStorageBlobMetadata.g.h"

// pdnet-hand: plain record of what the game asks to download (and the length once served).
namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageBlobMetadata : TitleStorageBlobMetadataT<TitleStorageBlobMetadata>
    {
        TitleStorageBlobMetadata() = default;

        TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId)
            : m_scid(serviceConfigurationId), m_storageType(storageType), m_blobPath(blobPath), m_blobType(blobType), m_xuid(xboxUserId) {}
        TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId, hstring const& displayName, hstring const& eTag)
            : TitleStorageBlobMetadata(serviceConfigurationId, storageType, blobPath, blobType, xboxUserId) { m_displayName = displayName; m_etag = eTag; }
        TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId, hstring const& displayName, hstring const& eTag, Windows::Foundation::DateTime const& clientTimestamp)
            : TitleStorageBlobMetadata(serviceConfigurationId, storageType, blobPath, blobType, xboxUserId, displayName, eTag) { m_timestamp = clientTimestamp; }

        static Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata CreateTitleStorageBlobMetadataForSessionStorage(hstring const& serviceConfigurationId, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& multiplayerSessionTemplateName, hstring const& multiplayerSessionName, hstring const& displayName, hstring const& eTag)
        {
            auto m = winrt::make_self<TitleStorageBlobMetadata>(serviceConfigurationId, TitleStorageType::SessionStorage, blobPath, blobType, hstring{}, displayName, eTag);
            m->m_sessionTemplate = multiplayerSessionTemplateName;
            m->m_sessionName = multiplayerSessionName;
            return *m;
        }

        hstring BlobPath() { return m_blobPath; }
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType BlobType() { return m_blobType; }
        Microsoft::Xbox::Services::TitleStorage::TitleStorageType StorageType() { return m_storageType; }
        hstring DisplayName() { return m_displayName; }
        hstring ETag() { return m_etag; }
        Windows::Foundation::DateTime ClientTimestamp() { return m_timestamp; }
        void set(Windows::Foundation::DateTime const& value) { m_timestamp = value; }
        uint64_t Length() { return m_length; }
        hstring ServiceConfigurationId() { return m_scid; }
        hstring XboxUserId() { return m_xuid; }
        hstring MultiplayerSessionTemplateName() { return m_sessionTemplate; }
        hstring MultiplayerSessionName() { return m_sessionName; }

        // pdnet: a copy of this record with the served length filled in.
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata WithLength(uint64_t length)
        {
            auto copy = winrt::make_self<TitleStorageBlobMetadata>(m_scid, m_storageType, m_blobPath, m_blobType, m_xuid, m_displayName, m_etag, m_timestamp);
            copy->m_sessionTemplate = m_sessionTemplate;
            copy->m_sessionName = m_sessionName;
            copy->m_length = length;
            return *copy;
        }

    private:
        hstring m_scid, m_blobPath, m_xuid, m_displayName, m_etag, m_sessionTemplate, m_sessionName;
        Microsoft::Xbox::Services::TitleStorage::TitleStorageType m_storageType{};
        Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType m_blobType{};
        Windows::Foundation::DateTime m_timestamp{};
        uint64_t m_length = 0;
    };
}
namespace winrt::Microsoft::Xbox::Services::TitleStorage::factory_implementation
{
    struct TitleStorageBlobMetadata : TitleStorageBlobMetadataT<TitleStorageBlobMetadata, implementation::TitleStorageBlobMetadata>
    {
    };
}
