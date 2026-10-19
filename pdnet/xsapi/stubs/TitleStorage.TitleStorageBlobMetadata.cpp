#include "xsapi_pch.h"
#include "TitleStorage.TitleStorageBlobMetadata.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    TitleStorageBlobMetadata::TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId)
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::TitleStorageBlobMetadata");
    }
    TitleStorageBlobMetadata::TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId, hstring const& displayName, hstring const& eTag)
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::TitleStorageBlobMetadata");
    }
    TitleStorageBlobMetadata::TitleStorageBlobMetadata(hstring const& serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType const& storageType, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& xboxUserId, hstring const& displayName, hstring const& eTag, Windows::Foundation::DateTime const& clientTimestamp)
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::TitleStorageBlobMetadata");
    }
    Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata TitleStorageBlobMetadata::CreateTitleStorageBlobMetadataForSessionStorage(hstring const& serviceConfigurationId, hstring const& blobPath, Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType const& blobType, hstring const& multiplayerSessionTemplateName, hstring const& multiplayerSessionName, hstring const& displayName, hstring const& eTag)
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::CreateTitleStorageBlobMetadataForSessionStorage");
    }
    hstring TitleStorageBlobMetadata::BlobPath()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::BlobPath");
    }
    Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobType TitleStorageBlobMetadata::BlobType()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::BlobType");
    }
    Microsoft::Xbox::Services::TitleStorage::TitleStorageType TitleStorageBlobMetadata::StorageType()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::StorageType");
    }
    hstring TitleStorageBlobMetadata::DisplayName()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::DisplayName");
    }
    hstring TitleStorageBlobMetadata::ETag()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::ETag");
    }
    Windows::Foundation::DateTime TitleStorageBlobMetadata::ClientTimestamp()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::ClientTimestamp");
    }
    void TitleStorageBlobMetadata::set(Windows::Foundation::DateTime const& value)
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::set");
    }
    uint64_t TitleStorageBlobMetadata::Length()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::Length");
    }
    hstring TitleStorageBlobMetadata::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::ServiceConfigurationId");
    }
    hstring TitleStorageBlobMetadata::XboxUserId()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::XboxUserId");
    }
    hstring TitleStorageBlobMetadata::MultiplayerSessionTemplateName()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::MultiplayerSessionTemplateName");
    }
    hstring TitleStorageBlobMetadata::MultiplayerSessionName()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadata::MultiplayerSessionName");
    }
}
