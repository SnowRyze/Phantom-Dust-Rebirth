#include "xsapi_pch.h"
#include "nametrace.h"
#include "mpsd.h"
#include <tlhelp32.h>
#include <algorithm>
#include <thread>

namespace pdnet::nametrace
{
    namespace
    {
        constexpr int kFrames = 8;
        constexpr int kMaxRecords = 512;
        constexpr uint64_t kGhidraBase = 0x140000000;

        // One distinct way the game read a watched name: which watch slot fired and the PDUWP.exe call chain.
        struct Record
        {
            int slot;
            uint64_t watched;
            uint64_t frames[kFrames];
            int frameCount;
            volatile LONG hits;
            bool logged;
        };

        Record g_records[kMaxRecords];
        volatile LONG g_recordCount = 0;
        SRWLOCK g_recordLock = SRWLOCK_INIT;
        uint64_t g_watched[4] = {}; // addresses currently watched (set before arming, read by the handler)
        uint64_t g_exeBase = 0, g_exeEnd = 0;

        bool InExe(uint64_t a) { return a >= g_exeBase && a < g_exeEnd; }
        uint64_t Ghidra(uint64_t a) { return a - g_exeBase + kGhidraBase; }

        // Walks the stack from the faulting context and keeps the return addresses inside PDUWP.exe.
        int Walk(CONTEXT ctx, uint64_t* out)
        {
            int n = 0;
            for (int i = 0; i < 40 && n < kFrames && ctx.Rip; ++i)
            {
                if (InExe(ctx.Rip)) out[n++] = ctx.Rip;
                DWORD64 imageBase = 0;
                PRUNTIME_FUNCTION fe = RtlLookupFunctionEntry(ctx.Rip, &imageBase, nullptr);
                if (!fe)
                {
                    ctx.Rip = *reinterpret_cast<DWORD64*>(ctx.Rsp); // leaf function: return address is on top
                    ctx.Rsp += 8;
                }
                else
                {
                    PVOID handlerData = nullptr;
                    DWORD64 establisher = 0;
                    RtlVirtualUnwind(UNW_FLAG_NHANDLER, imageBase, ctx.Rip, fe, &ctx, &handlerData, &establisher, nullptr);
                }
            }
            return n;
        }

        int SafeWalk(CONTEXT* ctx, uint64_t* out)
        {
            __try { return Walk(*ctx, out); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0; }
        }

        void Remember(int slot, const uint64_t* frames, int n)
        {
            AcquireSRWLockExclusive(&g_recordLock);
            LONG count = g_recordCount;
            for (LONG i = 0; i < count; ++i)
            {
                Record& r = g_records[i];
                if (r.watched == g_watched[slot] && r.frameCount == n && memcmp(r.frames, frames, n * sizeof(uint64_t)) == 0)
                {
                    ++r.hits;
                    ReleaseSRWLockExclusive(&g_recordLock);
                    return;
                }
            }
            if (count < kMaxRecords)
            {
                Record& r = g_records[count];
                r.slot = slot;
                r.watched = g_watched[slot];
                memcpy(r.frames, frames, n * sizeof(uint64_t));
                r.frameCount = n;
                r.hits = 1;
                r.logged = false;
                g_recordCount = count + 1;
            }
            ReleaseSRWLockExclusive(&g_recordLock);
        }

        // Hardware breakpoints arrive as single-step exceptions with the slot's bit set in DR6.
        LONG CALLBACK OnException(EXCEPTION_POINTERS* e)
        {
            if (e->ExceptionRecord->ExceptionCode != EXCEPTION_SINGLE_STEP) return EXCEPTION_CONTINUE_SEARCH;
            CONTEXT* c = e->ContextRecord;
            DWORD64 dr6 = c->Dr6;
            if ((dr6 & 0xF) == 0) return EXCEPTION_CONTINUE_SEARCH;
            int slot = 0;
            while (slot < 3 && !(dr6 & (1ull << slot))) ++slot;
            uint64_t frames[kFrames] = {};
            int n = SafeWalk(c, frames);
            if (n > 0) Remember(slot, frames, n);
            c->Dr6 = 0;
            return EXCEPTION_CONTINUE_EXECUTION;
        }

        // Sets DR0-DR3 (read/write watch, 8 bytes each) on every thread of the process except this one.
        void Arm(const uint64_t (&addrs)[4])
        {
            DWORD64 dr7 = 0;
            for (int i = 0; i < 4; ++i)
                if (addrs[i]) dr7 |= (1ull << (2 * i)) | (3ull << (16 + 4 * i)) | (2ull << (18 + 4 * i));

            std::vector<DWORD> threads;
            HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
            if (snap == INVALID_HANDLE_VALUE) return;
            THREADENTRY32 te{ sizeof(te) };
            for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te))
                if (te.th32OwnerProcessID == GetCurrentProcessId() && te.th32ThreadID != GetCurrentThreadId()) threads.push_back(te.th32ThreadID);
            CloseHandle(snap);

            std::vector<HANDLE> handles;
            for (DWORD tid : threads)
                if (HANDLE h = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, tid)) handles.push_back(h);

            // Nothing may allocate between suspend and resume: a suspended thread could hold the heap lock.
            for (HANDLE h : handles)
            {
                if (SuspendThread(h) == static_cast<DWORD>(-1)) continue;
                alignas(16) CONTEXT ctx{};
                ctx.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(h, &ctx))
                {
                    ctx.Dr0 = addrs[0];
                    ctx.Dr1 = addrs[1];
                    ctx.Dr2 = addrs[2];
                    ctx.Dr3 = addrs[3];
                    ctx.Dr6 = 0;
                    ctx.Dr7 = dr7;
                    SetThreadContext(h, &ctx);
                }
                ResumeThread(h);
            }
            for (HANDLE h : handles) CloseHandle(h);
        }

        struct Hit
        {
            uint64_t addr;
            std::wstring name;
            bool wide;
        };

        std::vector<Hit> Scan(const std::vector<std::wstring>& names)
        {
            struct Needle { std::string bytes; std::wstring name; bool wide; };
            std::vector<Needle> needles;
            for (auto& n : names)
            {
                needles.push_back({ std::string(reinterpret_cast<const char*>(n.data()), n.size() * 2), n, true });
                needles.push_back({ winrt::to_string(n), n, false });
            }

            constexpr size_t kChunk = 1 << 20;
            auto* buffer = static_cast<uint8_t*>(VirtualAlloc(nullptr, kChunk + 256, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
            if (!buffer) return {};
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&Scan), &self);

            std::vector<Hit> hits;
            MEMORY_BASIC_INFORMATION mbi{};
            for (uint8_t* p = nullptr; VirtualQuery(p, &mbi, sizeof(mbi)) == sizeof(mbi); p = static_cast<uint8_t*>(mbi.BaseAddress) + mbi.RegionSize)
            {
                const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
                if (mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_GUARD | PAGE_NOACCESS)) || !(mbi.Protect & writable)) continue;
                if (mbi.AllocationBase == buffer || mbi.AllocationBase == self) continue; // our scan buffer, pdnet's own data
                auto base = static_cast<uint8_t*>(mbi.BaseAddress);
                for (size_t off = 0; off < mbi.RegionSize && hits.size() < 400; off += kChunk)
                {
                    size_t want = (std::min)(kChunk + 128, mbi.RegionSize - off); // overlap so names across chunks are found
                    SIZE_T got = 0;
                    if (!ReadProcessMemory(GetCurrentProcess(), base + off, buffer, want, &got) || !got) continue;
                    for (auto& nd : needles)
                    {
                        auto first = reinterpret_cast<const char*>(buffer), last = first + got;
                        for (auto at = std::search(first, last, nd.bytes.begin(), nd.bytes.end()); at != last;
                             at = std::search(at + 1, last, nd.bytes.begin(), nd.bytes.end()))
                        {
                            size_t pos = static_cast<size_t>(at - first);
                            if (pos >= kChunk) break; // the overlap belongs to the next chunk
                            uint64_t addr = reinterpret_cast<uint64_t>(base + off + pos);
                            bool ours = false;
                            for (auto& other : needles)
                                if (addr >= reinterpret_cast<uint64_t>(other.bytes.data()) && addr < reinterpret_cast<uint64_t>(other.bytes.data()) + other.bytes.size()) ours = true;
                            if (!ours) hits.push_back({ addr, nd.name, nd.wide });
                        }
                    }
                }
            }
            VirtualFree(buffer, 0, MEM_RELEASE);
            // Copies inside the game's own data (static tables) first: those are the most likely to be what it draws.
            std::stable_sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return InExe(a.addr) && !InExe(b.addr); });
            return hits;
        }

        std::vector<std::wstring> RemoteNames()
        {
            std::vector<std::wstring> names;
            for (auto& doc : mpsd::SessionStore::Instance().All())
                for (auto& m : doc->members)
                    if (m.xuid != xsapi::LocalXuid() && m.gamertag.size() >= 3 &&
                        std::find(names.begin(), names.end(), m.gamertag) == names.end())
                        names.push_back(m.gamertag);
            return names;
        }

        std::wstring Where(uint64_t addr)
        {
            wchar_t text[64];
            if (InExe(addr)) swprintf_s(text, L"PDUWP.exe data %llx", Ghidra(addr));
            else swprintf_s(text, L"heap %llx", addr);
            return text;
        }

        void LogNewRecords(const std::vector<Hit>& hits)
        {
            AcquireSRWLockShared(&g_recordLock);
            LONG count = g_recordCount;
            for (LONG i = 0; i < count; ++i)
            {
                Record& r = g_records[i];
                if (r.logged) continue;
                r.logged = true;
                std::wstring chain, name = L"?";
                for (int f = 0; f < r.frameCount; ++f)
                {
                    wchar_t frame[24];
                    swprintf_s(frame, L"%s%llx", f ? L" <- " : L"", Ghidra(r.frames[f]));
                    chain += frame;
                }
                for (auto& h : hits)
                    if ((h.addr & ~7ull) == r.watched) name = h.name;
                Log(L"nametrace: '%s' at %s read by %s", name.c_str(), Where(r.watched).c_str(), chain.c_str());
            }
            ReleaseSRWLockShared(&g_recordLock);
        }

        void LogSummary()
        {
            AcquireSRWLockShared(&g_recordLock);
            for (LONG i = 0; i < g_recordCount; ++i)
            {
                Record& r = g_records[i];
                Log(L"nametrace: summary %s first frame %llx: %ld read(s)", Where(r.watched).c_str(), r.frameCount ? Ghidra(r.frames[0]) : 0, r.hits);
            }
            ReleaseSRWLockShared(&g_recordLock);
        }

        void Run()
        {
            winrt::init_apartment(winrt::apartment_type::multi_threaded);
            g_exeBase = reinterpret_cast<uint64_t>(GetModuleHandleW(nullptr));
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(g_exeBase + reinterpret_cast<IMAGE_DOS_HEADER*>(g_exeBase)->e_lfanew);
            g_exeEnd = g_exeBase + nt->OptionalHeader.SizeOfImage;
            AddVectoredExceptionHandler(1, OnException);
            Log(L"nametrace: on (PDUWP.exe at %llx, Ghidra addresses = 0x140000000 + offset); waiting for other players", g_exeBase);

            std::vector<std::wstring> names;
            std::vector<Hit> hits;
            ULONGLONG nextScan = 0, nextSummary = GetTickCount64() + 60000;
            size_t rotation = 0;
            bool armed = false;
            for (;;)
            {
                Sleep(3000);
                auto now = GetTickCount64();
                auto current = RemoteNames();
                if (current.empty())
                {
                    if (armed)
                    {
                        uint64_t none[4] = {};
                        g_watched[0] = g_watched[1] = g_watched[2] = g_watched[3] = 0;
                        Arm(none);
                        armed = false;
                        Log(L"nametrace: no other players; breakpoints off");
                    }
                    continue;
                }
                if (current != names || now >= nextScan)
                {
                    names = current;
                    hits = Scan(names);
                    nextScan = now + 30000;
                    rotation = 0;
                    size_t inExe = std::count_if(hits.begin(), hits.end(), [](const Hit& h) { return InExe(h.addr); });
                    Log(L"nametrace: %zu copies of %zu name(s) in memory (%zu in the game's own data)", hits.size(), names.size(), inExe);
                    for (size_t i = 0; i < hits.size() && i < 24; ++i)
                        Log(L"nametrace:   copy %zu: '%s' (%s) at %s", i, hits[i].name.c_str(), hits[i].wide ? L"UTF-16" : L"UTF-8", Where(hits[i].addr).c_str());
                }
                if (hits.empty()) continue;

                uint64_t addrs[4] = {};
                for (int i = 0; i < 4 && i < static_cast<int>(hits.size()); ++i)
                    addrs[i] = hits[(rotation + i) % hits.size()].addr & ~7ull;
                rotation = (rotation + 4) % hits.size();
                for (int i = 0; i < 4; ++i) g_watched[i] = addrs[i];
                Arm(addrs);
                armed = true;

                LogNewRecords(hits);
                if (now >= nextSummary)
                {
                    LogSummary();
                    nextSummary = now + 60000;
                }
            }
        }
    }

    void Start()
    {
        if (!GetConfig().nameTrace) return;
        static std::once_flag once;
        std::call_once(once, []
        {
            std::thread([]
            {
                try { Run(); }
                catch (...) { Log(L"nametrace: stopped (0x%08X)", winrt::to_hresult()); }
            }).detach();
        });
    }
}
