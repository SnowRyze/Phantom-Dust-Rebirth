#include "pch.h"
#include "gamesave.h"
#include "log.h"
#include <fstream>

namespace wf = winrt::Windows::Foundation;
namespace wfc = winrt::Windows::Foundation::Collections;
namespace wss = winrt::Windows::Storage::Streams;
namespace ws = winrt::Windows::Gaming::XboxLive::Storage;
namespace fs = std::filesystem;
using winrt::hstring;
using Status = ws::GameSaveErrorStatus;

namespace pdnet
{
    namespace
    {
        std::mutex g_io;
        constexpr int64_t kQuotaBytes = 256ll * 1024 * 1024;
        constexpr wchar_t kDisplayNameFile[] = L"_displayname";

        // Container and blob names may hold characters that are not valid in file names.
        std::wstring Escape(std::wstring_view name)
        {
            std::wstring out;
            for (wchar_t c : name)
            {
                if ((c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9') || c == L'.' || c == L'-')
                {
                    out += c;
                }
                else
                {
                    wchar_t hex[8];
                    swprintf_s(hex, L"_%04X", c);
                    out += hex;
                }
            }
            return out;
        }

        std::wstring Unescape(std::wstring_view name)
        {
            std::wstring out;
            for (size_t i = 0; i < name.size(); ++i)
            {
                if (name[i] == L'_' && i + 5 <= name.size())
                {
                    out += static_cast<wchar_t>(std::wcstoul(std::wstring(name.substr(i + 1, 4)).c_str(), nullptr, 16));
                    i += 4;
                }
                else
                {
                    out += name[i];
                }
            }
            return out;
        }

        fs::path SaveRoot()
        {
            return fs::path(LocalStatePath()) / L"pdnet" / L"saves";
        }

        fs::path ContainerDir(std::wstring_view container)
        {
            return SaveRoot() / Escape(container);
        }

        bool ReadFileBytes(const fs::path& path, std::vector<uint8_t>& bytes)
        {
            std::ifstream in(path, std::ios::binary);
            if (!in)
            {
                return false;
            }
            bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
            return true;
        }

        bool WriteFileAtomic(const fs::path& path, const uint8_t* data, size_t size)
        {
            std::error_code ec;
            fs::create_directories(path.parent_path(), ec);
            fs::path tmp = path;
            tmp += L".tmp";
            {
                std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
                if (!out)
                {
                    return false;
                }
                out.write(reinterpret_cast<const char*>(data), size);
                if (!out)
                {
                    return false;
                }
            }
            fs::rename(tmp, path, ec);
            return !ec;
        }

        std::wstring ReadDisplayName(const fs::path& dir)
        {
            std::vector<uint8_t> bytes;
            if (!ReadFileBytes(dir / kDisplayNameFile, bytes) || bytes.size() % 2)
            {
                return {};
            }
            return std::wstring(reinterpret_cast<const wchar_t*>(bytes.data()), bytes.size() / 2);
        }

        template <typename RuntimeClass, typename Impl, typename... Args>
        RuntimeClass Make(Args&&... args)
        {
            return winrt::make<Impl>(std::forward<Args>(args)...).template as<RuntimeClass>();
        }

        // ---------------------------------------------------------------- results

        struct OperationResult : winrt::implements<OperationResult, ws::IGameSaveOperationResult>
        {
            ws::GameSaveErrorStatus m_status;
            explicit OperationResult(ws::GameSaveErrorStatus s) : m_status(s) {}
            ws::GameSaveErrorStatus Status() const { return m_status; }
        };

        ws::GameSaveOperationResult OpResult(Status s)
        {
            return Make<ws::GameSaveOperationResult, OperationResult>(s);
        }

        struct BlobGetResult : winrt::implements<BlobGetResult, ws::IGameSaveBlobGetResult>
        {
            ws::GameSaveErrorStatus m_status;
            wfc::IMapView<hstring, wss::IBuffer> m_value;
            BlobGetResult(ws::GameSaveErrorStatus s, wfc::IMapView<hstring, wss::IBuffer> v) : m_status(s), m_value(std::move(v)) {}
            ws::GameSaveErrorStatus Status() const { return m_status; }
            wfc::IMapView<hstring, wss::IBuffer> Value() const { return m_value; }
        };

        // ---------------------------------------------------------------- blob info

        struct BlobInfo : winrt::implements<BlobInfo, ws::IGameSaveBlobInfo>
        {
            hstring m_name;
            uint32_t m_size;
            BlobInfo(hstring n, uint32_t s) : m_name(std::move(n)), m_size(s) {}
            hstring Name() const { return m_name; }
            uint32_t Size() const { return m_size; }
        };

        struct BlobInfoGetResult : winrt::implements<BlobInfoGetResult, ws::IGameSaveBlobInfoGetResult>
        {
            wfc::IVectorView<ws::GameSaveBlobInfo> m_value;
            explicit BlobInfoGetResult(wfc::IVectorView<ws::GameSaveBlobInfo> v) : m_value(std::move(v)) {}
            ws::GameSaveErrorStatus Status() const { return ws::GameSaveErrorStatus::Ok; }
            wfc::IVectorView<ws::GameSaveBlobInfo> Value() const { return m_value; }
        };

        struct BlobInfoQuery : winrt::implements<BlobInfoQuery, ws::IGameSaveBlobInfoQuery>
        {
            std::wstring m_container;
            std::wstring m_prefix;
            BlobInfoQuery(std::wstring c, std::wstring p) : m_container(std::move(c)), m_prefix(std::move(p)) {}

            std::vector<ws::GameSaveBlobInfo> List() const
            {
                std::lock_guard guard(g_io);
                std::vector<ws::GameSaveBlobInfo> items;
                std::error_code ec;
                for (auto& entry : fs::directory_iterator(ContainerDir(m_container), ec))
                {
                    std::wstring file = entry.path().filename().wstring();
                    if (!entry.is_regular_file() || file == kDisplayNameFile || entry.path().extension() == L".tmp")
                    {
                        continue;
                    }
                    std::wstring name = Unescape(file);
                    if (name.rfind(m_prefix, 0) == 0)
                    {
                        items.push_back(Make<ws::GameSaveBlobInfo, BlobInfo>(hstring{ name }, static_cast<uint32_t>(entry.file_size(ec))));
                    }
                }
                return items;
            }

            wf::IAsyncOperation<ws::GameSaveBlobInfoGetResult> GetBlobInfoAsync()
            {
                return GetBlobInfoAsync(0, UINT32_MAX);
            }

            wf::IAsyncOperation<ws::GameSaveBlobInfoGetResult> GetBlobInfoAsync(uint32_t start, uint32_t max)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                auto all = List();
                std::vector<ws::GameSaveBlobInfo> page;
                for (size_t i = start; i < all.size() && page.size() < max; ++i)
                {
                    page.push_back(all[i]);
                }
                co_return Make<ws::GameSaveBlobInfoGetResult, BlobInfoGetResult>(winrt::single_threaded_vector(std::move(page)).GetView());
            }

            wf::IAsyncOperation<uint32_t> GetItemCountAsync()
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                co_return static_cast<uint32_t>(List().size());
            }
        };

        // ---------------------------------------------------------------- containers

        struct Container : winrt::implements<Container, ws::IGameSaveContainer>
        {
            std::wstring m_name;
            ws::GameSaveProvider m_provider;
            Container(std::wstring n, ws::GameSaveProvider p) : m_name(std::move(n)), m_provider(std::move(p)) {}

            hstring Name() const { return hstring{ m_name }; }
            ws::GameSaveProvider Provider() const { return m_provider; }

            Status Write(const std::vector<std::pair<std::wstring, wss::IBuffer>>& writes, wfc::IIterable<hstring> const& deletes, std::wstring const& displayName) const
            {
                std::lock_guard guard(g_io);
                fs::path dir = ContainerDir(m_name);
                for (auto& [name, buffer] : writes)
                {
                    if (!WriteFileAtomic(dir / Escape(name), buffer ? buffer.data() : nullptr, buffer ? buffer.Length() : 0))
                    {
                        Log(L"GameSave: write FAILED %s/%s", m_name.c_str(), name.c_str());
                        return Status::OutOfLocalStorage;
                    }
                    Log(L"GameSave: wrote %s/%s (%u bytes)", m_name.c_str(), name.c_str(), buffer ? buffer.Length() : 0);
                }
                if (deletes)
                {
                    for (auto const& name : deletes)
                    {
                        std::error_code ec;
                        fs::remove(dir / Escape(name), ec);
                        Log(L"GameSave: deleted %s/%s", m_name.c_str(), name.c_str());
                    }
                }
                if (!displayName.empty())
                {
                    WriteFileAtomic(dir / kDisplayNameFile, reinterpret_cast<const uint8_t*>(displayName.data()), displayName.size() * sizeof(wchar_t));
                }
                return Status::Ok;
            }

            wf::IAsyncOperation<ws::GameSaveOperationResult> SubmitUpdatesAsync(wfc::IMapView<hstring, wss::IBuffer> blobsToWrite, wfc::IIterable<hstring> blobsToDelete, hstring displayName)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                std::vector<std::pair<std::wstring, wss::IBuffer>> writes;
                if (blobsToWrite)
                {
                    for (auto const& kv : blobsToWrite)
                    {
                        writes.emplace_back(std::wstring{ kv.Key() }, kv.Value());
                    }
                }
                co_return OpResult(Write(writes, blobsToDelete, std::wstring{ displayName }));
            }

            wf::IAsyncOperation<ws::GameSaveOperationResult> SubmitPropertySetUpdatesAsync(wfc::IPropertySet blobsToWrite, wfc::IIterable<hstring> blobsToDelete, hstring displayName)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                std::vector<std::pair<std::wstring, wss::IBuffer>> writes;
                if (blobsToWrite)
                {
                    for (auto const& kv : blobsToWrite)
                    {
                        writes.emplace_back(std::wstring{ kv.Key() }, kv.Value().try_as<wss::IBuffer>());
                    }
                }
                co_return OpResult(Write(writes, blobsToDelete, std::wstring{ displayName }));
            }

            wf::IAsyncOperation<ws::GameSaveOperationResult> ReadAsync(wfc::IMapView<hstring, wss::IBuffer> blobsToRead)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                std::lock_guard guard(g_io);
                for (auto const& kv : blobsToRead)
                {
                    std::vector<uint8_t> bytes;
                    std::wstring name{ kv.Key() };
                    if (!ReadFileBytes(ContainerDir(m_name) / Escape(name), bytes))
                    {
                        Log(L"GameSave: read %s/%s -> not found", m_name.c_str(), name.c_str());
                        co_return OpResult(Status::BlobNotFound);
                    }
                    auto buffer = kv.Value();
                    if (buffer.Capacity() < bytes.size())
                    {
                        Log(L"GameSave: read %s/%s -> buffer too small (%u < %zu)", m_name.c_str(), name.c_str(), buffer.Capacity(), bytes.size());
                        co_return OpResult(Status::ProvidedBufferTooSmall);
                    }
                    memcpy(buffer.data(), bytes.data(), bytes.size());
                    buffer.Length(static_cast<uint32_t>(bytes.size()));
                    Log(L"GameSave: read %s/%s (%zu bytes)", m_name.c_str(), name.c_str(), bytes.size());
                }
                co_return OpResult(Status::Ok);
            }

            wf::IAsyncOperation<ws::GameSaveBlobGetResult> GetAsync(wfc::IIterable<hstring> blobsToRead)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                std::lock_guard guard(g_io);
                std::map<hstring, wss::IBuffer> found;
                Status status = Status::Ok;
                for (auto const& name : blobsToRead)
                {
                    std::vector<uint8_t> bytes;
                    if (!ReadFileBytes(ContainerDir(m_name) / Escape(name), bytes))
                    {
                        Log(L"GameSave: get %s/%s -> not found", m_name.c_str(), name.c_str());
                        status = Status::BlobNotFound;
                        continue;
                    }
                    wss::Buffer buffer(static_cast<uint32_t>(bytes.size()));
                    memcpy(buffer.data(), bytes.data(), bytes.size());
                    buffer.Length(static_cast<uint32_t>(bytes.size()));
                    found.emplace(name, buffer);
                    Log(L"GameSave: get %s/%s (%zu bytes)", m_name.c_str(), name.c_str(), bytes.size());
                }
                co_return Make<ws::GameSaveBlobGetResult, BlobGetResult>(status, winrt::single_threaded_map(std::move(found)).GetView());
            }

            ws::GameSaveBlobInfoQuery CreateBlobInfoQuery(hstring const& prefix) const
            {
                return Make<ws::GameSaveBlobInfoQuery, BlobInfoQuery>(m_name, std::wstring{ prefix });
            }
        };

        struct ContainerInfo : winrt::implements<ContainerInfo, ws::IGameSaveContainerInfo>
        {
            hstring m_name, m_display;
            uint64_t m_size;
            wf::DateTime m_modified;
            ContainerInfo(hstring n, hstring d, uint64_t s, wf::DateTime m) : m_name(std::move(n)), m_display(std::move(d)), m_size(s), m_modified(m) {}
            hstring Name() const { return m_name; }
            uint64_t TotalSize() const { return m_size; }
            hstring DisplayName() const { return m_display; }
            wf::DateTime LastModifiedTime() const { return m_modified; }
            bool NeedsSync() const { return false; }
        };

        struct ContainerInfoGetResult : winrt::implements<ContainerInfoGetResult, ws::IGameSaveContainerInfoGetResult>
        {
            wfc::IVectorView<ws::GameSaveContainerInfo> m_value;
            explicit ContainerInfoGetResult(wfc::IVectorView<ws::GameSaveContainerInfo> v) : m_value(std::move(v)) {}
            ws::GameSaveErrorStatus Status() const { return ws::GameSaveErrorStatus::Ok; }
            wfc::IVectorView<ws::GameSaveContainerInfo> Value() const { return m_value; }
        };

        struct ContainerInfoQuery : winrt::implements<ContainerInfoQuery, ws::IGameSaveContainerInfoQuery>
        {
            std::wstring m_prefix;
            explicit ContainerInfoQuery(std::wstring p) : m_prefix(std::move(p)) {}

            std::vector<ws::GameSaveContainerInfo> List() const
            {
                std::lock_guard guard(g_io);
                std::vector<ws::GameSaveContainerInfo> items;
                std::error_code ec;
                for (auto& dir : fs::directory_iterator(SaveRoot(), ec))
                {
                    if (!dir.is_directory())
                    {
                        continue;
                    }
                    std::wstring name = Unescape(dir.path().filename().wstring());
                    if (name.rfind(m_prefix, 0) != 0)
                    {
                        continue;
                    }
                    uint64_t total = 0;
                    fs::file_time_type newest{};
                    for (auto& f : fs::directory_iterator(dir.path(), ec))
                    {
                        if (f.is_regular_file() && f.path().filename() != kDisplayNameFile)
                        {
                            total += f.file_size(ec);
                            newest = (std::max)(newest, f.last_write_time(ec));
                        }
                    }
                    // MSVC's file_clock counts 100ns ticks since 1601, the same as FILETIME.
                    auto modified = winrt::clock::from_file_time(winrt::file_time{ static_cast<uint64_t>(newest.time_since_epoch().count()) });
                    items.push_back(Make<ws::GameSaveContainerInfo, ContainerInfo>(hstring{ name }, hstring{ ReadDisplayName(dir.path()) }, total, modified));
                }
                return items;
            }

            wf::IAsyncOperation<ws::GameSaveContainerInfoGetResult> GetContainerInfoAsync()
            {
                return GetContainerInfoAsync(0, UINT32_MAX);
            }

            wf::IAsyncOperation<ws::GameSaveContainerInfoGetResult> GetContainerInfoAsync(uint32_t start, uint32_t max)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                auto all = List();
                std::vector<ws::GameSaveContainerInfo> page;
                for (size_t i = start; i < all.size() && page.size() < max; ++i)
                {
                    page.push_back(all[i]);
                }
                Log(L"GameSave: container query '%s' -> %zu", m_prefix.c_str(), page.size());
                co_return Make<ws::GameSaveContainerInfoGetResult, ContainerInfoGetResult>(winrt::single_threaded_vector(std::move(page)).GetView());
            }

            wf::IAsyncOperation<uint32_t> GetItemCountAsync()
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                co_return static_cast<uint32_t>(List().size());
            }
        };

        // ---------------------------------------------------------------- provider

        struct Provider : winrt::implements<Provider, ws::IGameSaveProvider>
        {
            winrt::Windows::System::User m_user;
            explicit Provider(winrt::Windows::System::User u) : m_user(std::move(u)) {}

            winrt::Windows::System::User User() const { return m_user; }

            ws::GameSaveContainer CreateContainer(hstring const& name)
            {
                Log(L"GameSave: CreateContainer('%s')", name.c_str());
                ws::IGameSaveProvider self = *this;
                return Make<ws::GameSaveContainer, Container>(std::wstring{ name }, self.as<ws::GameSaveProvider>());
            }

            wf::IAsyncOperation<ws::GameSaveOperationResult> DeleteContainerAsync(hstring name)
            {
                auto strong = get_strong();
                co_await winrt::resume_background();
                std::lock_guard guard(g_io);
                std::error_code ec;
                fs::remove_all(ContainerDir(name), ec);
                Log(L"GameSave: DeleteContainer('%s')", name.c_str());
                co_return OpResult(Status::Ok);
            }

            ws::GameSaveContainerInfoQuery CreateContainerInfoQuery() const
            {
                return CreateContainerInfoQuery(L"");
            }

            ws::GameSaveContainerInfoQuery CreateContainerInfoQuery(hstring const& prefix) const
            {
                return Make<ws::GameSaveContainerInfoQuery, ContainerInfoQuery>(std::wstring{ prefix });
            }

            wf::IAsyncOperation<int64_t> GetRemainingBytesInQuotaAsync()
            {
                co_return kQuotaBytes;
            }

            wfc::IVectorView<hstring> ContainersChangedSinceLastSync() const
            {
                return winrt::single_threaded_vector<hstring>().GetView();
            }
        };

        struct ProviderGetResult : winrt::implements<ProviderGetResult, ws::IGameSaveProviderGetResult>
        {
            ws::GameSaveProvider m_value;
            explicit ProviderGetResult(ws::GameSaveProvider v) : m_value(std::move(v)) {}
            ws::GameSaveErrorStatus Status() const { return ws::GameSaveErrorStatus::Ok; }
            ws::GameSaveProvider Value() const { return m_value; }
        };

        struct ProviderFactory : winrt::implements<ProviderFactory, wf::IActivationFactory, ws::IGameSaveProviderStatics>
        {
            wf::IInspectable ActivateInstance() const
            {
                throw winrt::hresult_not_implemented();
            }

            wf::IAsyncOperation<ws::GameSaveProviderGetResult> GetForUserAsync(winrt::Windows::System::User user, hstring scid)
            {
                Log(L"GameSave: GetForUserAsync(scid=%s) -> local provider at %s", scid.c_str(), SaveRoot().c_str());
                auto provider = Make<ws::GameSaveProvider, Provider>(user);
                co_return Make<ws::GameSaveProviderGetResult, ProviderGetResult>(provider);
            }

            wf::IAsyncOperation<ws::GameSaveProviderGetResult> GetSyncOnDemandForUserAsync(winrt::Windows::System::User user, hstring scid)
            {
                return GetForUserAsync(user, scid);
            }
        };
    }

    wf::IActivationFactory CreateGameSaveProviderFactory()
    {
        return winrt::make<ProviderFactory>();
    }
}
