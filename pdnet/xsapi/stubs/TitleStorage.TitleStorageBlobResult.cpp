#include "xsapi_pch.h"
#include "TitleStorage.TitleStorageBlobResult.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    Windows::Storage::Streams::IBuffer TitleStorageBlobResult::BlobBuffer()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobResult::BlobBuffer");
    }
    Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata TitleStorageBlobResult::BlobMetadata()
    {
        PDNET_XSAPI_STUB("TitleStorageBlobResult::BlobMetadata");
    }
}
