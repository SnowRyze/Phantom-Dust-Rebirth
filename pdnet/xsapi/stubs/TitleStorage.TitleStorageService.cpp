#include "xsapi_pch.h"
#include "TitleStorage.TitleStorageService.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageQuota> TitleStorageService::GetQuotaAsync(hstring serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType storageType)
    {
        PDNET_XSAPI_STUB("TitleStorageService::GetQuotaAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageQuota> TitleStorageService::GetQuotaForSessionStorageAsync(hstring serviceConfigurationId, hstring multiplayerSessionTemplateName, hstring multiplayerSessionName)
    {
        PDNET_XSAPI_STUB("TitleStorageService::GetQuotaForSessionStorageAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> TitleStorageService::GetBlobMetadataAsync(hstring serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType storageType, hstring blobPath, hstring xboxUserId, uint32_t skipItems, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("TitleStorageService::GetBlobMetadataAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> TitleStorageService::GetBlobMetadataForSessionStorageAsync(hstring serviceConfigurationId, hstring blobPath, hstring multiplayerSessionTemplateName, hstring multiplayerSessionName, uint32_t skipItems, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("TitleStorageService::GetBlobMetadataForSessionStorageAsync");
    }
    Windows::Foundation::IAsyncAction TitleStorageService::DeleteBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, bool deleteOnlyIfETagMatches)
    {
        PDNET_XSAPI_STUB("TitleStorageService::DeleteBlobAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> TitleStorageService::DownloadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, hstring selectQuery)
    {
        PDNET_XSAPI_STUB("TitleStorageService::DownloadBlobAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> TitleStorageService::DownloadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, hstring selectQuery, uint32_t preferredDownloadBlockSize)
    {
        PDNET_XSAPI_STUB("TitleStorageService::DownloadBlobAsync");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata> TitleStorageService::UploadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, uint32_t preferredUploadBlockSize)
    {
        PDNET_XSAPI_STUB("TitleStorageService::UploadBlobAsync");
    }
}
