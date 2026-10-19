#include "xsapi_pch.h"
#include "TitleStorage.TitleStorageQuota.h"


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    hstring TitleStorageQuota::ServiceConfigurationId()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::ServiceConfigurationId");
    }
    Microsoft::Xbox::Services::TitleStorage::TitleStorageType TitleStorageQuota::StorageType()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::StorageType");
    }
    hstring TitleStorageQuota::XboxUserId()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::XboxUserId");
    }
    hstring TitleStorageQuota::MultiplayerSessionTemplateName()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::MultiplayerSessionTemplateName");
    }
    hstring TitleStorageQuota::MultiplayerSessionName()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::MultiplayerSessionName");
    }
    uint64_t TitleStorageQuota::UsedBytes()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::UsedBytes");
    }
    uint64_t TitleStorageQuota::QuotaBytes()
    {
        PDNET_XSAPI_STUB("TitleStorageQuota::QuotaBytes");
    }
}
