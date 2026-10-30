// pdnet-hand: DownloadBlobAsync serves files locally; the other members are still generated defaults.
#include "xsapi_pch.h"
#include "TitleStorage.TitleStorageService.h"
#include "TitleStorage.TitleStorageQuota.h"
#include "TitleStorage.TitleStorageBlobResult.h"
#include "TitleStorage.TitleStorageBlobMetadataResult.h"
#include "TitleStorage.TitleStorageBlobMetadata.h"
#include <fstream>


namespace winrt::Microsoft::Xbox::Services::TitleStorage::implementation
{
    namespace
    {
        std::wstring PackageDir()
        {
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&PackageDir), &self);
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(self, path, MAX_PATH);
            std::wstring dir = path;
            return dir.substr(0, dir.find_last_of(L'\\') + 1);
        }

        const wchar_t* StorageName(TitleStorageType t)
        {
            switch (t)
            {
            case TitleStorageType::GlobalStorage: return L"global";
            case TitleStorageType::TrustedPlatformStorage: return L"trusted";
            case TitleStorageType::UntrustedPlatformStorage: return L"untrusted";
            case TitleStorageType::JsonStorage: return L"json";
            case TitleStorageType::SessionStorage: return L"session";
            default: return L"universal";
            }
        }

        // Where a blob can come from, first match wins:
        //   <build>\titlestorage\<storage>\<path>   (e.g. titlestorage\global\gsdata_en.dat - optional overrides)
        //   <build>\titlestorage\<path>
        //   <build>\Assets\Data\gstorage\<path>     (the game's own offline copies of its global storage)
        //   <build>\Assets\Data\<path>
        bool LoadBlob(TitleStorageType type, std::wstring path, std::vector<uint8_t>& bytes, std::wstring& from)
        {
            std::replace(path.begin(), path.end(), L'/', L'\\');
            while (!path.empty() && path[0] == L'\\') path.erase(0, 1);
            if (path.find(L"..") != std::wstring::npos) return false;
            std::wstring dir = PackageDir();
            for (const std::wstring& candidate : {
                     dir + L"titlestorage\\" + StorageName(type) + L"\\" + path,
                     dir + L"titlestorage\\" + path,
                     dir + L"Assets\\Data\\gstorage\\" + path,
                     dir + L"Assets\\Data\\" + path })
            {
                std::ifstream in(candidate, std::ios::binary);
                if (!in) continue;
                bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                from = candidate.substr(dir.size());
                return true;
            }
            return false;
        }

        Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> Download(
            Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata metadata, Windows::Storage::Streams::IBuffer buffer)
        {
            co_await winrt::resume_background();
            std::wstring path{ metadata.BlobPath() };
            std::vector<uint8_t> bytes;
            std::wstring from;
            if (!LoadBlob(metadata.StorageType(), path, bytes, from))
            {
                ::pdnet::Log(L"xsapi-pdnet: title storage %s '%s' -> no local copy, reporting 'not found'", StorageName(metadata.StorageType()), path.c_str());
                throw winrt::hresult_error(HTTP_E_STATUS_NOT_FOUND, L"pdnet: no local copy of this title storage blob");
            }

            // Like XSAPI: fill the caller's buffer if it is big enough, otherwise hand back a new one.
            if (!buffer || buffer.Capacity() < bytes.size())
            {
                ::pdnet::Log(L"xsapi-pdnet: title storage buffer too small (%u < %zu), using a new one", buffer ? buffer.Capacity() : 0u, bytes.size());
                buffer = Windows::Storage::Streams::Buffer(static_cast<uint32_t>(bytes.size()));
            }
            if (!bytes.empty()) memcpy(buffer.data(), bytes.data(), bytes.size());
            buffer.Length(static_cast<uint32_t>(bytes.size()));

            ::pdnet::Log(L"xsapi-pdnet: title storage %s '%s' -> %zu bytes from %s", StorageName(metadata.StorageType()), path.c_str(), bytes.size(), from.c_str());
            auto served = winrt::get_self<implementation::TitleStorageBlobMetadata>(metadata)->WithLength(bytes.size());
            co_return winrt::make<implementation::TitleStorageBlobResult>(buffer, served);
        }
    }

    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageQuota> TitleStorageService::GetQuotaAsync(hstring serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType storageType)
    {
        PDNET_XSAPI_STUB_DEFAULT("TitleStorageService::GetQuotaAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::TitleStorage::implementation::TitleStorageQuota>();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageQuota> TitleStorageService::GetQuotaForSessionStorageAsync(hstring serviceConfigurationId, hstring multiplayerSessionTemplateName, hstring multiplayerSessionName)
    {
        PDNET_XSAPI_STUB_DEFAULT("TitleStorageService::GetQuotaForSessionStorageAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::TitleStorage::implementation::TitleStorageQuota>();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> TitleStorageService::GetBlobMetadataAsync(hstring serviceConfigurationId, Microsoft::Xbox::Services::TitleStorage::TitleStorageType storageType, hstring blobPath, hstring xboxUserId, uint32_t skipItems, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB_DEFAULT("TitleStorageService::GetBlobMetadataAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::TitleStorage::implementation::TitleStorageBlobMetadataResult>();
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadataResult> TitleStorageService::GetBlobMetadataForSessionStorageAsync(hstring serviceConfigurationId, hstring blobPath, hstring multiplayerSessionTemplateName, hstring multiplayerSessionName, uint32_t skipItems, uint32_t maxItems)
    {
        PDNET_XSAPI_STUB_DEFAULT("TitleStorageService::GetBlobMetadataForSessionStorageAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::TitleStorage::implementation::TitleStorageBlobMetadataResult>();
    }
    Windows::Foundation::IAsyncAction TitleStorageService::DeleteBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, bool deleteOnlyIfETagMatches)
    {
        PDNET_XSAPI_STUB_DEFAULT("TitleStorageService::DeleteBlobAsync"); co_return;
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> TitleStorageService::DownloadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, hstring selectQuery)
    {
        return Download(blobMetadata, blobBuffer);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobResult> TitleStorageService::DownloadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, hstring selectQuery, uint32_t preferredDownloadBlockSize)
    {
        return Download(blobMetadata, blobBuffer);
    }
    Windows::Foundation::IAsyncOperation<Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata> TitleStorageService::UploadBlobAsync(Microsoft::Xbox::Services::TitleStorage::TitleStorageBlobMetadata blobMetadata, Windows::Storage::Streams::IBuffer blobBuffer, Microsoft::Xbox::Services::TitleStorage::TitleStorageETagMatchCondition etagMatchCondition, uint32_t preferredUploadBlockSize)
    {
        PDNET_XSAPI_STUB_DEFAULT("TitleStorageService::UploadBlobAsync"); co_return winrt::make<winrt::Microsoft::Xbox::Services::TitleStorage::implementation::TitleStorageBlobMetadata>();
    }
}
