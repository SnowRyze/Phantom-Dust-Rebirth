#include "pch.h"
#include "film.h"
#include "gamepatches.h"
#include "log.h"
#include <algorithm>
#include <cstdio>
#include <map>
#include <mutex>
#include <vector>

// Conceptual branch only: Theatre films, step 1 (docs\CONCEPTUAL.md, "Theatre"). Records each match as a film of what drove it
// (strategy B of the replay spec: the state the PCs exchange, not inputs): every game datagram this PC's game sends and
// receives over EOS P2P (p2p.cpp), with the time and the game's frame counter, plus a snapshot of the match setup. Phantom Dust
// has no server: each PC runs its own player and sends the others its updates (26 streams, see docs\CONCEPTUAL.md), so a film
// is one PC's view of the match. Playback is a later step; this only writes.
//
// File: LocalState\pdnet\films\film-<date>-<time>.pdfilm, append-only, little-endian.
//   Header: "PDFILM\0\0", u32 format version, u32 game exe TimeDateStamp, u32 game exe SizeOfImage, u64 start (FILETIME, UTC),
//           u16 length + the kit label (kit-version.txt's first line).
//   Records: u8 type, u8 peer, u16 payload length, u32 ms since the start, i32 game frame (DAT_1404bc63c), payload.
//     1 packet received from the peer, 2 packet sent to the peer: payload = the datagram.
//     3 peer: payload = its EOS product user id (the peer number is this record's own).
//     4 match state: payload = i32 (0x1404bc950).
//     5 snapshot of the match setup (Snapshot below), when the match first reaches state 6.
//     255 end: payload = u32 records, u64 packet bytes, u32 dropped records.
// Disk: a background thread writes the buffer about once a second (no flush each time); the buffer is capped, and records past
// the cap are dropped and counted, never waiting on the disk. The newest 25 films are kept.
namespace pdnet::film
{
    namespace
    {
        constexpr uint64_t kGhidraBase = 0x140000000;
        uint8_t* At(uint64_t ghidra) { return reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr)) + (ghidra - kGhidraBase); }
        template <typename T> T Var(uint64_t ghidra) { return *reinterpret_cast<volatile T*>(At(ghidra)); }

        constexpr uint32_t kFormat = 1;
        constexpr size_t kBufferCap = 8 * 1024 * 1024;
        constexpr size_t kKeepFilms = 25;
        enum : uint8_t { kIn = 1, kOut = 2, kPeer = 3, kState = 4, kSnapshot = 5, kEnd = 255 };

        std::mutex g_lock;              // the buffer and the peer table
        std::vector<uint8_t> g_buffer;  // records not yet written
        std::map<std::string, uint8_t> g_peers;
        std::atomic<bool> g_recording{ false };
        ULONGLONG g_start = 0;
        uint32_t g_records = 0, g_dropped = 0;
        uint64_t g_packetBytes = 0;

        std::wstring FilmsDir() { return LocalStatePath() + L"\\pdnet\\films"; }

        template <typename T> void Put(std::vector<uint8_t>& b, T v) { const uint8_t* p = reinterpret_cast<const uint8_t*>(&v); b.insert(b.end(), p, p + sizeof(T)); }

        // With g_lock held.
        void AddRecord(uint8_t type, uint8_t peer, const void* payload, size_t bytes)
        {
            if (bytes > 0xffff) bytes = 0xffff;
            if (g_buffer.size() + 12 + bytes > kBufferCap) { ++g_dropped; return; }
            Put<uint8_t>(g_buffer, type);
            Put<uint8_t>(g_buffer, peer);
            Put<uint16_t>(g_buffer, static_cast<uint16_t>(bytes));
            Put<uint32_t>(g_buffer, static_cast<uint32_t>(GetTickCount64() - g_start));
            Put<int32_t>(g_buffer, Var<int32_t>(0x1404bc63c));
            const uint8_t* p = static_cast<const uint8_t*>(payload);
            g_buffer.insert(g_buffer.end(), p, p + bytes);
            ++g_records;
        }

        std::string KitLabel()
        {
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&KitLabel), &self);
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(self, path, MAX_PATH);
            std::wstring file = path;
            file = file.substr(0, file.find_last_of(L'\\') + 1) + L"kit-version.txt";
            std::string label;
            if (FILE* f = nullptr; _wfopen_s(&f, file.c_str(), L"rb") == 0 && f)
            {
                char line[256] = {};
                if (fgets(line, sizeof(line), f)) label = line;
                fclose(f);
            }
            if (label.rfind("\xEF\xBB\xBF", 0) == 0) label.erase(0, 3);
            while (!label.empty() && (label.back() == '\n' || label.back() == '\r')) label.pop_back();
            return label;
        }

#pragma pack(push, 1)
        struct SlotInfo
        {
            uint8_t present;
            int32_t team;    // player object +0x1c
            int32_t health;  // status record +0xc
            int32_t target;  // status record +0x60: its index among the targetable objects
            char name[0x41]; // DAT_1404bca30 + slot * 0x41
        };
        struct Snapshot
        {
            uint32_t version = 1;
            int32_t gameType, rule, stage, layout, localSlot, mode, test;
            SlotInfo slots[4];
            uint8_t localArsenal[100]; // this PC's arsenal for the match (save list + index at save +0x3b08)
        };
#pragma pack(pop)

        bool TakeSnapshot(Snapshot& s)
        {
            __try
            {
                s.gameType = Var<int32_t>(0x1405277b4);
                s.rule = Var<int32_t>(0x1405277fc);
                s.stage = Var<int32_t>(0x1403e0304);
                s.layout = Var<uint8_t>(0x140531dcf);
                s.localSlot = Var<int32_t>(0x1404bbe00);
                s.mode = Var<int32_t>(0x1404bc9d4);
                s.test = gamepatches::TestBattle() ? 1 : 0;
                for (int i = 0; i < 4; ++i)
                {
                    uint8_t* object = reinterpret_cast<uint8_t**>(At(0x1404bc9d8))[i];
                    uint8_t* record = reinterpret_cast<uint8_t**>(At(0x1404bc608))[i];
                    SlotInfo& slot = s.slots[i];
                    slot.present = object ? 1 : 0;
                    slot.team = object ? *reinterpret_cast<int32_t*>(object + 0x1c) : -1;
                    slot.health = record ? *reinterpret_cast<int32_t*>(record + 0xc) : 0;
                    slot.target = record ? *reinterpret_cast<int32_t*>(record + 0x60) : -1;
                    memcpy(slot.name, At(0x1404bca30) + i * 0x41, 0x41);
                    slot.name[0x40] = 0;
                }
                uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
                uint8_t* list = save ? *reinterpret_cast<uint8_t**>(save + 0x10a * 0x38) : nullptr;
                int index = save ? *reinterpret_cast<int32_t*>(save + 0x3b08) : -1;
                if (list && index >= 0 && index < 16) memcpy(s.localArsenal, list + index * 100, 100);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Keeps the newest kKeepFilms films (by name, which starts with the date and time).
        void Prune()
        {
            std::vector<std::wstring> films;
            WIN32_FIND_DATAW fd;
            HANDLE find = FindFirstFileW((FilmsDir() + L"\\film-*.pdfilm").c_str(), &fd);
            if (find == INVALID_HANDLE_VALUE) return;
            do films.push_back(fd.cFileName); while (FindNextFileW(find, &fd));
            FindClose(find);
            std::sort(films.begin(), films.end());
            for (size_t i = 0; i + kKeepFilms < films.size(); ++i)
            {
                DeleteFileW((FilmsDir() + L"\\" + films[i]).c_str());
                Log(L"film: oldest film removed (%zu kept): %s", kKeepFilms, films[i].c_str());
            }
        }

        std::mutex g_fileLock; // g_file: the writer thread and the watcher (opening, closing)
        FILE* g_file = nullptr;
        std::wstring g_name;

        // Writes what is buffered (the writer thread, and the end of a film).
        void Drain()
        {
            std::vector<uint8_t> out;
            {
                std::lock_guard guard(g_lock);
                out.swap(g_buffer);
                g_buffer.reserve(256 * 1024);
            }
            std::lock_guard file(g_fileLock);
            if (g_file && !out.empty() && fwrite(out.data(), 1, out.size(), g_file) != out.size())
            {
                static int told = 0;
                if (told++ < 3) Log(L"film: writing %s failed", g_name.c_str());
            }
        }

        bool OpenFilm()
        {
            CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
            CreateDirectoryW(FilmsDir().c_str(), nullptr);
            SYSTEMTIME t;
            GetLocalTime(&t);
            wchar_t name[64];
            swprintf_s(name, L"film-%04u%02u%02u-%02u%02u%02u.pdfilm", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
            g_name = name;
            if (_wfopen_s(&g_file, (FilmsDir() + L"\\" + g_name).c_str(), L"wb") != 0 || !g_file)
            {
                g_file = nullptr;
                Log(L"film: %s could not be created", g_name.c_str());
                return false;
            }
            std::vector<uint8_t> header;
            const char magic[8] = { 'P', 'D', 'F', 'I', 'L', 'M', 0, 0 };
            header.insert(header.end(), magic, magic + 8);
            Put<uint32_t>(header, kFormat);
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(GetModuleHandleW(nullptr));
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<uint8_t*>(dos) + dos->e_lfanew);
            Put<uint32_t>(header, nt->FileHeader.TimeDateStamp);
            Put<uint32_t>(header, nt->OptionalHeader.SizeOfImage);
            FILETIME now;
            GetSystemTimeAsFileTime(&now);
            Put<uint64_t>(header, (static_cast<uint64_t>(now.dwHighDateTime) << 32) | now.dwLowDateTime);
            std::string label = KitLabel();
            Put<uint16_t>(header, static_cast<uint16_t>(label.size()));
            header.insert(header.end(), label.begin(), label.end());
            fwrite(header.data(), 1, header.size(), g_file);
            {
                std::lock_guard guard(g_lock);
                g_buffer.clear();
                g_peers.clear();
                g_records = g_dropped = 0;
                g_packetBytes = 0;
                g_start = GetTickCount64();
            }
            g_recording = true;
            Log(L"film: recording %s", g_name.c_str());
            return true;
        }

        void CloseFilm()
        {
            g_recording = false;
            uint32_t records, dropped;
            uint64_t bytes;
            ULONGLONG ms;
            {
                std::lock_guard guard(g_lock);
                uint8_t end[16];
                records = g_records; bytes = g_packetBytes; dropped = g_dropped;
                memcpy(end, &records, 4); memcpy(end + 4, &bytes, 8); memcpy(end + 12, &dropped, 4);
                AddRecord(kEnd, 0, end, sizeof(end));
                ms = GetTickCount64() - g_start;
            }
            Drain();
            long size = 0;
            {
                std::lock_guard file(g_fileLock);
                size = g_file ? ftell(g_file) : 0;
                if (g_file) fclose(g_file);
                g_file = nullptr;
            }
            double minutes = ms / 60000.0;
            Log(L"film: %s saved: %.1f min, %u records, %llu KB of packets, %ld KB file (%.0f KB a minute)%s", g_name.c_str(), minutes, records,
                bytes / 1024, size / 1024, minutes > 0 ? size / 1024.0 / minutes : 0.0, dropped ? L", SOME RECORDS DROPPED (buffer full)" : L"");
            if (dropped) Log(L"film: %u records dropped", dropped);
            Prune();
        }
    }

    void Packet(bool out, const std::string& peer, const uint8_t* data, size_t bytes)
    {
        if (!g_recording) return;
        std::lock_guard guard(g_lock);
        auto it = g_peers.find(peer);
        uint8_t index;
        if (it == g_peers.end())
        {
            if (g_peers.size() >= 250) return;
            index = static_cast<uint8_t>(g_peers.size() + 1);
            g_peers[peer] = index;
            AddRecord(kPeer, index, peer.data(), peer.size());
        }
        else index = it->second;
        AddRecord(out ? kOut : kIn, index, data, bytes);
        g_packetBytes += bytes;
    }

    void Start()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            std::thread([] // the writer: about once a second
            {
                for (;;)
                {
                    Sleep(1000);
                    if (g_recording) Drain();
                }
            }).detach();
            std::thread([] // the watcher: a film per match
            {
                int last = -1;
                bool snapshot = false;
                for (;;)
                {
                    Sleep(50);
                    int state = Var<int32_t>(0x1404bc950);
                    bool match = state != 0 && state != 13;
                    if (match && !g_recording) { snapshot = false; OpenFilm(); }
                    if (g_recording && state != last)
                    {
                        std::lock_guard guard(g_lock);
                        AddRecord(kState, 0, &state, sizeof(state));
                    }
                    if (g_recording && state == 6 && !snapshot)
                    {
                        Snapshot s{};
                        if (TakeSnapshot(s))
                        {
                            std::lock_guard guard(g_lock);
                            AddRecord(kSnapshot, 0, &s, sizeof(s));
                        }
                        snapshot = true;
                        Log(L"film: match setup recorded (type %d, rule %d, stage %d, layout %d, this PC slot %d%s)", s.gameType, s.rule, s.stage, s.layout,
                            s.localSlot, s.test ? L", Test Arsenal" : L"");
                    }
                    if (!match && g_recording) CloseFilm();
                    last = state;
                }
            }).detach();
            Log(L"film: recorder ready (each match is saved to LocalState\\pdnet\\films, the newest %zu kept)", kKeepFilms);
        });
    }
}
