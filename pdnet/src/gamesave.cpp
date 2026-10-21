#include "pch.h"
#include "gamesave.h"
#include "log.h"
#include <fstream>
#include <condition_variable>
#include <winrt/Windows.System.h>
#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.UI.Notifications.h>
#include <winrt/Windows.Data.Xml.Dom.h>

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
        // After an import the game's in-memory copy of the save is the old one; its writes are ignored until the game
        // restarts, so it can't overwrite what was imported.
        std::atomic<bool> g_writesBlocked{ false };

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
                if (g_writesBlocked)
                {
                    Log(L"GameSave: not saving %s - an Xbox save was imported; restart the game to play with it", m_name.c_str());
                    return Status::Ok;
                }
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

        // ---------------------------------------------------------------- import from Xbox (the Help button)

        // The player and title the game opened its saves for (GetForUserAsync), for the import.
        std::mutex g_userLock;
        winrt::Windows::System::User g_user{ nullptr };
        hstring g_scid;
        std::atomic<bool> g_importing{ false };

        // Waits for an Xbox call, at most `seconds` (an unpacked copy can hang forever trying to sign in).
        template <typename Async>
        bool Finished(Async const& op, int seconds)
        {
            if (op.wait_for(std::chrono::seconds(seconds)) == wf::AsyncStatus::Completed) return true;
            op.Cancel();
            return false;
        }

        void Notify(const std::wstring& title, const std::wstring& text)
        {
            try
            {
                namespace wun = winrt::Windows::UI::Notifications;
                auto xml = wun::ToastNotificationManager::GetTemplateContent(wun::ToastTemplateType::ToastText02);
                auto lines = xml.GetElementsByTagName(L"text");
                lines.Item(0).AppendChild(xml.CreateTextNode(title));
                lines.Item(1).AppendChild(xml.CreateTextNode(text));
                wun::ToastNotificationManager::CreateToastNotifier().Show(wun::ToastNotification(xml));
            }
            catch (...)
            {
                Log(L"GameSave: couldn't show a notification (0x%08X)", winrt::to_hresult());
            }
        }

        // Copies every container and blob of the player's Xbox cloud save (story progress, collection, arsenals,
        // play time) into `target` (a fresh folder, not the live saves), through the real Xbox save service. Read
        // only: the cloud save is not changed. Stops writing once `cancelled` is set. Returns an empty string on
        // success, else what went wrong.
        std::wstring CopyCloudSave(winrt::Windows::System::User const& user, hstring const& scid, fs::path const& target,
                                   std::atomic<bool> const& cancelled, size_t& containers, size_t& blobs, size_t& bytes)
        {
            auto getProvider = ws::GameSaveProvider::GetForUserAsync(user, scid); // the real service: pdnet's own calls aren't hooked
            if (!Finished(getProvider, 20)) return L"Xbox didn't answer in 20 s (signed in to Xbox?)";
            auto got = getProvider.GetResults();
            if (got.Status() != Status::Ok) return L"Xbox save service status " + std::to_wstring(static_cast<int>(got.Status()));
            auto provider = got.Value();
            auto queryInfos = provider.CreateContainerInfoQuery().GetContainerInfoAsync();
            if (!Finished(queryInfos, 30)) return L"listing the cloud save timed out";
            auto infos = queryInfos.GetResults();
            if (infos.Status() != Status::Ok) return L"listing the cloud save failed, status " + std::to_wstring(static_cast<int>(infos.Status()));
            for (auto const& info : infos.Value())
            {
                std::wstring name{ info.Name() };
                auto container = provider.CreateContainer(info.Name());
                auto queryBlobs = container.CreateBlobInfoQuery(L"").GetBlobInfoAsync();
                if (!Finished(queryBlobs, 30) || queryBlobs.GetResults().Status() != Status::Ok) return L"listing container " + name + L" failed";
                auto names = winrt::single_threaded_vector<hstring>();
                for (auto const& blob : queryBlobs.GetResults().Value()) names.Append(blob.Name());
                if (names.Size() == 0) continue;
                auto read = container.GetAsync(names.GetView());
                if (!Finished(read, 60) || read.GetResults().Status() != Status::Ok) return L"reading container " + name + L" failed";
                if (cancelled) return L"cancelled";
                fs::path dir = target / Escape(name);
                for (auto const& blob : read.GetResults().Value())
                {
                    auto buffer = blob.Value();
                    if (!WriteFileAtomic(dir / Escape(blob.Key()), buffer ? buffer.data() : nullptr, buffer ? buffer.Length() : 0))
                        return L"writing " + name + L"/" + std::wstring(blob.Key()) + L" failed";
                    ++blobs;
                    bytes += buffer ? buffer.Length() : 0;
                    Log(L"GameSave: import: %s/%s (%u bytes)", name.c_str(), blob.Key().c_str(), buffer ? buffer.Length() : 0);
                }
                std::wstring display{ info.DisplayName() };
                if (!display.empty()) WriteFileAtomic(dir / kDisplayNameFile, reinterpret_cast<const uint8_t*>(display.data()), display.size() * sizeof(wchar_t));
                ++containers;
            }
            return containers ? L"" : L"the Xbox cloud save is empty";
        }

        // The whole import, off the game's thread. The cloud save is read into saves.xbox-import next to the live saves;
        // only after a complete read do the folders swap (live saves -> a dated backup folder, import -> saves). The
        // Xbox calls can hang outright (e.g. an unpacked copy waiting for a sign-in), so they run on their own thread
        // and the import gives up after 90 s, leaving the live saves untouched.
        void ImportCloudSave()
        {
            if (g_importing.exchange(true)) return;
            std::thread([]
            {
                winrt::Windows::System::User user{ nullptr };
                hstring scid;
                {
                    std::lock_guard guard(g_userLock);
                    user = g_user;
                    scid = g_scid;
                }
                fs::path root = SaveRoot(), staging = root;
                staging += L".xbox-import";
                std::wstring problem;
                size_t containers = 0, blobs = 0, bytes = 0;
                bool store = false;
                try { store = winrt::Windows::ApplicationModel::Package::Current().SignatureKind() == winrt::Windows::ApplicationModel::PackageSignatureKind::Store; }
                catch (...) {}
                if (!user) problem = L"the game hasn't opened its saves yet";
                // An unpacked copy can't sign in to Xbox, and waking the real save service there was followed by a crash.
                else if (!store) problem = L"this needs the Microsoft Store version of Phantom Dust, signed in to Xbox";
                else
                {
                    Log(L"GameSave: import: reading the Xbox cloud save into %s ...", staging.c_str());
                    std::error_code ec;
                    fs::remove_all(staging, ec); // a leftover from an earlier attempt
                    struct Shared { std::mutex lock; std::condition_variable done; bool finished = false; std::wstring problem; std::atomic<bool> cancelled{ false }; size_t c = 0, b = 0, n = 0; };
                    auto shared = std::make_shared<Shared>();
                    std::thread([shared, user, scid, staging]
                    {
                        std::wstring result;
                        size_t c = 0, b = 0, n = 0;
                        try { result = CopyCloudSave(user, scid, staging, shared->cancelled, c, b, n); }
                        catch (...) { result = L"Xbox save service error 0x" + std::to_wstring(static_cast<uint32_t>(winrt::to_hresult())); }
                        std::lock_guard guard(shared->lock);
                        shared->problem = result;
                        shared->c = c, shared->b = b, shared->n = n;
                        shared->finished = true;
                        shared->done.notify_all();
                    }).detach();
                    std::unique_lock wait(shared->lock);
                    if (!shared->done.wait_for(wait, std::chrono::seconds(90), [&] { return shared->finished; }))
                    {
                        shared->cancelled = true;
                        problem = L"Xbox didn't answer in 90 s (signed in to Xbox? an unpacked copy usually can't be)";
                    }
                    else
                    {
                        problem = shared->problem;
                        containers = shared->c, blobs = shared->b, bytes = shared->n;
                    }
                    wait.unlock();
                    if (problem.empty())
                    {
                        g_writesBlocked = true; // from here on the game's in-memory save is stale
                        SYSTEMTIME t;
                        GetLocalTime(&t);
                        wchar_t name[64];
                        swprintf_s(name, L"saves.before-xbox-import-%04u%02u%02u-%02u%02u%02u", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
                        fs::path backup = root.parent_path() / name;
                        std::lock_guard guard(g_io);
                        if (fs::exists(root, ec)) fs::rename(root, backup, ec);
                        if (ec) problem = L"couldn't move the current saves aside (" + winrt::to_hstring(ec.message()) + L")";
                        else
                        {
                            fs::rename(staging, root, ec);
                            if (ec)
                            {
                                problem = L"couldn't put the imported save in place (" + winrt::to_hstring(ec.message()) + L")";
                                std::error_code ec2;
                                fs::rename(backup, root, ec2);
                            }
                            else Log(L"GameSave: import: previous saves kept in %s", backup.c_str());
                        }
                        if (!problem.empty()) g_writesBlocked = false;
                    }
                }
                if (problem.empty())
                {
                    Log(L"GameSave: import done - %zu container(s), %zu blob(s), %zu bytes. Saving is paused until the game restarts.", containers, blobs, bytes);
                    Notify(L"Xbox save imported", L"Restart Phantom Dust to play with it. Saving is paused until then.");
                }
                else
                {
                    Log(L"GameSave: import FAILED - %s. The current saves are unchanged.", problem.c_str());
                    Notify(L"Xbox save import failed", problem);
                }
                g_importing = false;
            }).detach();
        }

        // Stands in for Windows.System.Launcher: the old Help & Support button opens the Phantom Dust page on
        // xbox.com; that request starts the Xbox save import instead. Everything else goes to the real Launcher.
        struct LauncherFactory : winrt::implements<LauncherFactory, wf::IActivationFactory, winrt::Windows::System::ILauncherStatics>
        {
            winrt::Windows::System::ILauncherStatics m_real{ winrt::get_activation_factory<winrt::Windows::System::Launcher, winrt::Windows::System::ILauncherStatics>() };

            wf::IInspectable ActivateInstance() const { throw winrt::hresult_not_implemented(); }

            static bool IsHelpPage(wf::Uri const& uri)
            {
                std::wstring host{ uri ? uri.Host() : hstring{} };
                for (auto& c : host) c = static_cast<wchar_t>(towlower(c));
                return host == L"xbox.com" || (host.size() > 9 && host.compare(host.size() - 9, 9, L".xbox.com") == 0);
            }

            wf::IAsyncOperation<bool> LaunchFileAsync(winrt::Windows::Storage::IStorageFile const& file) const { return m_real.LaunchFileAsync(file); }
            wf::IAsyncOperation<bool> LaunchFileAsync(winrt::Windows::Storage::IStorageFile const& file, winrt::Windows::System::LauncherOptions const& options) const { return m_real.LaunchFileAsync(file, options); }

            wf::IAsyncOperation<bool> LaunchUriAsync(wf::Uri const& uri) const
            {
                if (!IsHelpPage(uri)) return m_real.LaunchUriAsync(uri);
                Log(L"GameSave: Help button (%s) -> importing the Xbox save", uri.AbsoluteUri().c_str());
                ImportCloudSave();
                return Done();
            }

            wf::IAsyncOperation<bool> LaunchUriAsync(wf::Uri const& uri, winrt::Windows::System::LauncherOptions const& options) const
            {
                if (!IsHelpPage(uri)) return m_real.LaunchUriAsync(uri, options);
                return LaunchUriAsync(uri);
            }

            static wf::IAsyncOperation<bool> Done() { co_return true; }
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
                {
                    std::lock_guard guard(g_userLock);
                    g_user = user;
                    g_scid = scid;
                }
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

    wf::IActivationFactory CreateLauncherFactory()
    {
        return winrt::make<LauncherFactory>();
    }
}
