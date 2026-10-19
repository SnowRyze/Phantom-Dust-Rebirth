#include "xsapi_pch.h"
#include "TitleStorage.TitleStorageBlobMetadataResult.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    Windows::Foundation::Collections::IVectorView<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata> TitleStorageBlobMetadataResult::Items()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadataResult::Items");
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> TitleStorageBlobMetadataResult::GetNextAsync(uint32_t maxItems)
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadataResult::GetNextAsync");
    }
    bool TitleStorageBlobMetadataResult::HasNext()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobMetadataResult::HasNext");
    }
}
