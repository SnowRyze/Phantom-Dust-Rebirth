#pragma once
#include "TitleStorage/TitleStorageService.g.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    struct TitleStorageService : TitleStorageServiceT<TitleStorageService>
    {
        TitleStorageService() = default;

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageQuota> GetQuotaAsync(hstring serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType storageType);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageQuota> GetQuotaForSessionStorageAsync(hstring serviceConfigurationId, hstring multiplayerSessionTemplateName, hstring multiplayerSessionName);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> GetBlobMetadataAsync(hstring serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType storageType, hstring blobPath, hstring xboxUserId, uint32_t skipItems, uint32_t maxItems);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> GetBlobMetadataForSessionStorageAsync(hstring serviceConfigurationId, hstring blobPath, hstring multiplayerSessionTemplateName, hstring multiplayerSessionName, uint32_t skipItems, uint32_t maxItems);
        Windows::Foundation::IAsyncAction DeleteBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, bool deleteOnlyIfETagMatches);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> DownloadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, hstring selectQuery);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> DownloadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, hstring selectQuery, uint32_t preferredDownloadBlockSize);
        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata> UploadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, uint32_t preferredUploadBlockSize);
    };
}
