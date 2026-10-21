#include "pch.h"
#include "gamepatches.h"
#include "config.h"
#include "log.h"

namespace pdnet::gamepatches
{
    namespace
    {
        constexpr uint64_t kGhidraBase = 0x140000000;

        std::atomic<uint8_t> g_bodyTags{ 0 }; // read by the game's nametag code through the routine below

        uint8_t* Image() { return reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr)); }
        uint8_t* At(uint64_t ghidra) { return Image() + (ghidra - kGhidraBase); }

        // Free space for a few bytes of extra code: the zero-filled tail of the last page of PDUWP.exe's .text
        // section (mapped executable, never used by the game).
        uint8_t* CodeCave(size_t size)
        {
            static size_t used = 0; // caves handed out so far (each rounded up to 16 bytes)
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(Image());
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(Image() + dos->e_lfanew);
            auto section = IMAGE_FIRST_SECTION(nt);
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
            {
                if (memcmp(section->Name, ".text", 5) != 0) continue;
                uint64_t start = (section->VirtualAddress + section->Misc.VirtualSize + 15) & ~15ull;
                uint64_t pageEnd = (section->VirtualAddress + section->Misc.VirtualSize + 0xfff) & ~0xfffull;
                if (pageEnd - start < used + size + 16) return nullptr;
                uint8_t* cave = Image() + start + 16 + used; // keep a little distance from the last real code
                for (size_t k = 0; k < size; ++k)
                    if (cave[k] != 0) return nullptr;
                used += (size + 15) & ~size_t{ 15 };
                return cave;
            }
            return nullptr;
        }

        bool Write(uint8_t* target, const std::vector<uint8_t>& bytes)
        {
            DWORD old = 0;
            if (!VirtualProtect(target, bytes.size(), PAGE_EXECUTE_READWRITE, &old)) return false;
            memcpy(target, bytes.data(), bytes.size());
            VirtualProtect(target, bytes.size(), old, &old);
            FlushInstructionCache(GetCurrentProcess(), target, bytes.size());
            return true;
        }

        void Put32(std::vector<uint8_t>& b, int32_t v) { for (int i = 0; i < 4; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
        void Put64(std::vector<uint8_t>& b, uint64_t v) { for (int i = 0; i < 8; ++i) b.push_back(static_cast<uint8_t>(v >> (8 * i))); }
        int32_t Rel32(const uint8_t* from, const uint8_t* to) { return static_cast<int32_t>(to - from); }

        // The nametag is drawn white: right before the draw call the game stores 0xffffffff in its text colour
        // (two qwords at 0x14054c9b0 / 0x14054c9b8). Those two stores become a call to a small routine that stores
        // `argb` instead, while nametags on dead bodies are on (g_bodyTags, the lobby host's setting), for a player
        // (status record = DAT_1404bc608[r15]) who is dead (health +0x0c is 0) or playing dead: the Play Dead skill
        // (id 360) is their current action (+0x4b4) and that action is still running (+0x4ac = 1; getting up or being
        // hit changes both). A fake death then looks like a real one.
        void DeadNametagColor(uint32_t argb)
        {
            constexpr uint64_t kSite = 0x1401512cf;
            constexpr int32_t kPlayDead = 360;
            const std::vector<uint8_t> expected = { 0x48, 0xc7, 0x05, 0xd6, 0xb6, 0x3f, 0x00, 0xff, 0xff, 0xff, 0xff,
                                                    0x48, 0xc7, 0x05, 0xd3, 0xb6, 0x3f, 0x00, 0xff, 0xff, 0xff, 0xff };
            uint8_t* site = At(kSite);
            if (memcmp(site, expected.data(), expected.size()) != 0)
            {
                Log(L"gamepatch: dead nametag colour skipped - the game code is not what this pdnet expects");
                return;
            }
            constexpr size_t kCaveSize = 128;
            uint8_t* cave = CodeCave(kCaveSize);
            if (!cave)
            {
                Log(L"gamepatch: dead nametag colour skipped - no free space for the colour routine");
                return;
            }

            std::vector<uint8_t> code;
            auto rip = [&](size_t instructionLength) { return cave + code.size() + instructionLength; };
            std::vector<size_t> toWhite, toDead; // rel8 jumps to patch once the labels are known
            auto jump = [&](uint8_t opcode, std::vector<size_t>& to) { code.push_back(opcode); to.push_back(code.size()); code.push_back(0); };

            code.insert(code.end(), { 0x48, 0xb8 });                 // mov rax, &g_bodyTags
            Put64(code, reinterpret_cast<uint64_t>(&g_bodyTags));
            code.insert(code.end(), { 0x80, 0x38, 0x00 });           // cmp byte [rax], 0
            jump(0x74, toWhite);                                     // je white
            code.insert(code.end(), { 0x48, 0x8d, 0x05 });           // lea rax, [DAT_1404bc608]
            Put32(code, Rel32(rip(4), At(0x1404bc608)));
            code.insert(code.end(), { 0x4a, 0x8b, 0x04, 0xf8 });     // mov rax, [rax + r15*8]
            code.insert(code.end(), { 0x48, 0x85, 0xc0 });           // test rax, rax
            jump(0x74, toWhite);                                     // jz white
            code.insert(code.end(), { 0x83, 0x78, 0x0c, 0x00 });     // cmp dword [rax+0xc], 0
            jump(0x74, toDead);                                      // je dead
            code.insert(code.end(), { 0x81, 0xb8, 0xb4, 0x04, 0x00, 0x00 }); // cmp dword [rax+0x4b4], 360
            Put32(code, kPlayDead);
            jump(0x75, toWhite);                                     // jne white
            code.insert(code.end(), { 0x83, 0xb8, 0xac, 0x04, 0x00, 0x00, 0x01 }); // cmp dword [rax+0x4ac], 1
            jump(0x75, toWhite);                                     // jne white
            size_t dead = code.size();
            code.insert(code.end(), { 0x48, 0xb8 });                 // dead: mov rax, colour
            Put64(code, (static_cast<uint64_t>(argb) << 32) | argb);
            size_t toStore = code.size() + 1;
            code.insert(code.end(), { 0xeb, 0x00 });                 // jmp store
            size_t white = code.size();
            code.insert(code.end(), { 0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff }); // white: mov rax, -1
            size_t store = code.size();
            code.insert(code.end(), { 0x48, 0x89, 0x05 });           // store: mov [0x14054c9b0], rax
            Put32(code, Rel32(rip(4), At(0x14054c9b0)));
            code.insert(code.end(), { 0x48, 0x89, 0x05 });           // mov [0x14054c9b8], rax
            Put32(code, Rel32(rip(4), At(0x14054c9b8)));
            code.push_back(0xc3);                                    // ret
            for (size_t at : toWhite) code[at] = static_cast<uint8_t>(white - (at + 1));
            for (size_t at : toDead) code[at] = static_cast<uint8_t>(dead - (at + 1));
            code[toStore] = static_cast<uint8_t>(store - (toStore + 1));

            std::vector<uint8_t> call = { 0xe8 };
            Put32(call, Rel32(site + 5, cave));
            call.resize(expected.size(), 0x90);

            if (!Write(cave, code) || !Write(site, call))
            {
                Log(L"gamepatch: dead nametag colour failed (VirtualProtect error %lu)", GetLastError());
                return;
            }
            Log(L"gamepatch: dead nametag colour applied (ARGB %08X, also while playing dead)", argb);
        }

        // FUN_140150c30 draws the white nametag over each other player only while their health (status record +0x0c)
        // is not zero:   cmp dword ptr [rcx+0xc],0 / jz skip_nametag   (10 bytes at 0x1401511be)
        // Those become a jump to a routine that skips the health check while g_bodyTags is 1, so the name stays on a
        // player at 0 health (a dead body). g_bodyTags can change at any time (the lobby host's setting, see header).
        // The checks after it are kept: status effect 8 (e.g. invisibility), on screen, +0x54/+0x5c.
        bool NametagsOnBodies()
        {
            constexpr uint64_t kSite = 0x1401511be;
            const std::vector<uint8_t> expected = { 0x83, 0x79, 0x0c, 0x00, 0x0f, 0x84, 0x84, 0x01, 0x00, 0x00 };
            uint8_t* site = At(kSite);
            if (memcmp(site, expected.data(), expected.size()) != 0)
            {
                Log(L"gamepatch: nametags on dead bodies skipped - the game code is not what this pdnet expects (different game version?)");
                return false;
            }
            uint8_t* next = site + expected.size(); // 0x1401511c8
            uint8_t* skip = next + 0x184;           // the jz target
            uint8_t* cave = CodeCave(48);
            if (!cave)
            {
                Log(L"gamepatch: nametags on dead bodies skipped - no free space for the check routine");
                return false;
            }
            std::vector<uint8_t> code;
            auto rip = [&](size_t instructionLength) { return cave + code.size() + instructionLength; };
            code.insert(code.end(), { 0x50, 0x48, 0xb8 });                  // push rax ; mov rax, &g_bodyTags
            Put64(code, reinterpret_cast<uint64_t>(&g_bodyTags));
            code.insert(code.end(), { 0x80, 0x38, 0x00, 0x58 });            // cmp byte [rax], 0 ; pop rax
            code.insert(code.end(), { 0x0f, 0x85 });                        // jne next (dead bodies keep their name)
            Put32(code, Rel32(rip(4), next));
            code.insert(code.end(), { 0x83, 0x79, 0x0c, 0x00, 0x0f, 0x84 }); // cmp dword [rcx+0xc], 0 ; je skip
            Put32(code, Rel32(rip(4), skip));
            code.push_back(0xe9);                                           // jmp next
            Put32(code, Rel32(rip(4), next));

            std::vector<uint8_t> jump = { 0xe9 };
            Put32(jump, Rel32(site + 5, cave));
            jump.resize(expected.size(), 0x90);
            if (!Write(cave, code) || !Write(site, jump))
            {
                Log(L"gamepatch: nametags on dead bodies failed (VirtualProtect error %lu)", GetLastError());
                return false;
            }
            Log(L"gamepatch: nametags on dead bodies installed (now %s)", g_bodyTags ? L"on" : L"off");
            return true;
        }
    }

    namespace
    {
        // nametag_debug=on: the fields FUN_140150c30 checks before drawing a player's nametag, per player status
        // record (DAT_1404bc608[i]): health +0x0c, +0x54, +0x58, +0x5c, +0x60, the +0xf8 pointer and what it points
        // to, and the 16 status-effect values (+0x61b8/+0x61bc per 0xc entry; effect 8 hides the nametag).
        constexpr int kFields = 7, kEffects = 16;
        struct Snapshot { int32_t f[kFields]; int32_t effect[kEffects]; };

        bool ReadSnapshot(const uint8_t* rec, Snapshot& s)
        {
            __try
            {
                const int offsets[] = { 0x0c, 0x54, 0x58, 0x5c, 0x60 };
                for (int k = 0; k < 5; ++k) s.f[k] = *reinterpret_cast<const int32_t*>(rec + offsets[k]);
                auto p = *reinterpret_cast<int32_t* const*>(rec + 0xf8);
                s.f[5] = p != nullptr;
                s.f[6] = p ? *p : 0;
                for (int k = 0; k < kEffects; ++k)
                    s.effect[k] = *reinterpret_cast<const int32_t*>(rec + 0x61b8 + k * 0xc) + *reinterpret_cast<const int32_t*>(rec + 0x61bc + k * 0xc);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool CopyMemory32(const uint8_t* from, int32_t* to, size_t count)
        {
            __try
            {
                memcpy(to, from, count * 4);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // nametag_debug=deep: also every int32 of player 1's status record (DAT_1404bc608[0], 0x6400 bytes) and
        // player object (DAT_1404bc9d8[0], 0x14000 bytes) that changes rarely: each offset is logged for its first
        // 8 changes, so constantly changing values (positions, timers) drop out within a second.
        void DeepDebug()
        {
            std::thread([]
            {
                struct Area { const wchar_t* name; uint64_t table; size_t count; std::vector<int32_t> last; std::vector<uint8_t> changes; bool have = false; };
                Area areas[] = { { L"status", 0x1404bc608, 0x6400 / 4 }, { L"object", 0x1404bc9d8, 0x14000 / 4 } };
                Log(L"nametag-debug: deep mode, logging rare changes in player 1's status record and player object");
                std::vector<int32_t> now;
                for (;;)
                {
                    Sleep(50);
                    for (auto& a : areas)
                    {
                        auto base = *reinterpret_cast<uint8_t**>(At(a.table));
                        now.resize(a.count);
                        if (!base || !CopyMemory32(base, now.data(), a.count))
                        {
                            a.have = false;
                            continue;
                        }
                        if (!a.have)
                        {
                            a.last = now;
                            a.changes.assign(a.count, 0);
                            a.have = true;
                            continue;
                        }
                        for (size_t k = 0; k < a.count; ++k)
                        {
                            if (now[k] == a.last[k]) continue;
                            if (a.changes[k] < 8)
                            {
                                ++a.changes[k];
                                Log(L"nametag-debug: %s+0x%X %d -> %d (0x%X)", a.name, static_cast<unsigned>(k * 4), a.last[k], now[k], now[k]);
                            }
                            a.last[k] = now[k];
                        }
                    }
                }
            }).detach();
        }

        void NametagDebug()
        {
            if (GetConfig().nametagDebug > 1) DeepDebug();
            std::thread([]
            {
                Log(L"nametag-debug: logging nametag fields of each player when they change");
                Snapshot last[4]{};
                bool had[4]{};
                for (;;)
                {
                    Sleep(100);
                    for (int i = 0; i < 4; ++i)
                    {
                        auto rec = *reinterpret_cast<uint8_t**>(At(0x1404bc608) + i * 8);
                        Snapshot s{};
                        if (!rec || !ReadSnapshot(rec, s))
                        {
                            had[i] = false;
                            continue;
                        }
                        if (had[i] && memcmp(&s, &last[i], sizeof(s)) == 0) continue;
                        wchar_t effects[256] = {};
                        for (int k = 0; k < kEffects; ++k)
                            if (s.effect[k]) swprintf_s(effects + wcslen(effects), _countof(effects) - wcslen(effects), L" e%d=%d", k, s.effect[k]);
                        char name[0x41] = {};
                        memcpy(name, At(0x1404bca30) + i * 0x41, 0x40);
                        Log(L"nametag-debug: P%d '%S' hp=%d 54=%d 58=0x%X 5c=%d 60=%d f8=%s%d effects:%s", i + 1, name, s.f[0], s.f[1], s.f[2], s.f[3], s.f[4],
                            s.f[5] ? L"*" : L"null ", s.f[6], effects[0] ? effects : L" none");
                        last[i] = s;
                        had[i] = true;
                    }
                }
            }).detach();
        }
    }

    void Apply()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            g_bodyTags = GetConfig().nametagsOnBodies ? 1 : 0;
            if (NametagsOnBodies()) DeadNametagColor(GetConfig().deadNametagColor);
            if (GetConfig().nametagDebug) NametagDebug();
        });
    }

    void SetNametagsOnBodies(bool on, const wchar_t* why)
    {
        uint8_t want = on ? 1 : 0;
        if (g_bodyTags.exchange(want) != want) Log(L"gamepatch: nametags on dead bodies %s (%s)", on ? L"on" : L"off", why);
    }

    void UseOwnSettings() { SetNametagsOnBodies(GetConfig().nametagsOnBodies, L"this PC's pdnet.ini"); }
}
