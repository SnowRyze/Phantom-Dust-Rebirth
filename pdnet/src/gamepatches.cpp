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
        std::atomic<uint8_t> g_playDead[4]{}; // per player slot: lying in Play Dead (TrackPlayDead), read by the colour routine

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
        // who is dead (health +0x0c of DAT_1404bc608[r15], the record the game's health check uses) or playing dead
        // (g_playDead[slot], set by TrackPlayDead, for r15 or the drawn slot rbx; the two differ in some battle types).
        // A fake death then looks like a real one.
        void DeadNametagColor(uint32_t argb)
        {
            constexpr uint64_t kSite = 0x1401512cf;
            const std::vector<uint8_t> expected = { 0x48, 0xc7, 0x05, 0xd6, 0xb6, 0x3f, 0x00, 0xff, 0xff, 0xff, 0xff,
                                                    0x48, 0xc7, 0x05, 0xd3, 0xb6, 0x3f, 0x00, 0xff, 0xff, 0xff, 0xff };
            uint8_t* site = At(kSite);
            if (memcmp(site, expected.data(), expected.size()) != 0)
            {
                Log(L"gamepatch: dead nametag colour skipped - the game code is not what this pdnet expects");
                return;
            }
            constexpr size_t kCaveSize = 256;
            uint8_t* cave = CodeCave(kCaveSize);
            if (!cave)
            {
                Log(L"gamepatch: dead nametag colour skipped - no free space for the colour routine");
                return;
            }

            std::vector<uint8_t> code;
            auto rip = [&](size_t instructionLength) { return cave + code.size() + instructionLength; };
            // Jumps are emitted with 32-bit offsets (patched once the labels are known): jcc = 0F 8x, jmp = E9.
            std::vector<size_t> toWhite, toDead, toNext;
            auto jump = [&](uint8_t condition, std::vector<size_t>& to)
            {
                if (condition) code.insert(code.end(), { 0x0f, condition });
                else code.push_back(0xe9);
                to.push_back(code.size());
                Put32(code, 0);
            };
            constexpr uint8_t kJe = 0x84, kJne = 0x85, kJa = 0x87, kJmp = 0;
            auto label = [&](std::vector<size_t>& from)
            {
                for (size_t at : from)
                {
                    int32_t rel = static_cast<int32_t>(code.size() - (at + 4));
                    memcpy(&code[at], &rel, 4);
                }
                from.clear();
            };
            code.insert(code.end(), { 0x48, 0xb8 });                 // mov rax, &g_bodyTags
            Put64(code, reinterpret_cast<uint64_t>(&g_bodyTags));
            code.insert(code.end(), { 0x80, 0x38, 0x00 });           // cmp byte [rax], 0
            jump(kJe, toWhite);
            code.insert(code.end(), { 0x48, 0x8d, 0x05 });           // lea rax, [DAT_1404bc608]
            Put32(code, Rel32(rip(4), At(0x1404bc608)));
            code.insert(code.end(), { 0x4a, 0x8b, 0x04, 0xf8 });     // mov rax, [rax + r15*8]
            code.insert(code.end(), { 0x48, 0x85, 0xc0 });           // test rax, rax
            jump(kJe, toWhite);
            code.insert(code.end(), { 0x83, 0x78, 0x0c, 0x00 });     // cmp dword [rax+0xc], 0
            jump(kJe, toDead);
            code.insert(code.end(), { 0x48, 0xb8 });                 // mov rax, &g_playDead
            Put64(code, reinterpret_cast<uint64_t>(&g_playDead[0]));
            code.insert(code.end(), { 0x49, 0x83, 0xff, 0x03 });     // cmp r15, 3
            jump(kJa, toNext);
            code.insert(code.end(), { 0x42, 0x80, 0x3c, 0x38, 0x00 }); // cmp byte [rax + r15], 0
            jump(kJne, toDead);
            label(toNext);
            code.insert(code.end(), { 0x83, 0xfb, 0x03 });           // cmp ebx, 3 (the drawn slot, as the game checks it)
            jump(kJa, toWhite);
            code.insert(code.end(), { 0x80, 0x3c, 0x18, 0x00 });     // cmp byte [rax + rbx], 0
            jump(kJne, toDead);
            jump(kJmp, toWhite);
            label(toDead);
            code.insert(code.end(), { 0x48, 0xb8 });                 // dead: mov rax, colour
            Put64(code, (static_cast<uint64_t>(argb) << 32) | argb);
            std::vector<size_t> toStore;
            jump(kJmp, toStore);
            label(toWhite);
            code.insert(code.end(), { 0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff }); // white: mov rax, -1
            label(toStore);
            code.insert(code.end(), { 0x48, 0x89, 0x05 });           // store: mov [0x14054c9b0], rax
            Put32(code, Rel32(rip(4), At(0x14054c9b0)));
            code.insert(code.end(), { 0x48, 0x89, 0x05 });           // mov [0x14054c9b8], rax
            Put32(code, Rel32(rip(4), At(0x14054c9b8)));
            code.push_back(0xc3);                                    // ret
            if (code.size() > kCaveSize) { Log(L"gamepatch: dead nametag colour skipped - routine larger than its space"); return; }

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
        constexpr int kFields = 9, kStates = 9, kEffects = 16;
        constexpr int kStateOffsets[kStates] = { 0x8, 0x3bc, 0x3cc, 0x4a0, 0x4a4, 0x4a8, 0x4b0, 0x504, 0x61a8 };
        struct Snapshot { int32_t f[kFields]; int32_t state[kStates]; int32_t effect[kEffects]; };

        bool ReadSnapshot(const uint8_t* rec, Snapshot& s)
        {
            __try
            {
                const int offsets[] = { 0x0c, 0x54, 0x58, 0x5c, 0x60 };
                for (int k = 0; k < 5; ++k) s.f[k] = *reinterpret_cast<const int32_t*>(rec + offsets[k]);
                auto p = *reinterpret_cast<int32_t* const*>(rec + 0xf8);
                s.f[5] = p != nullptr;
                s.f[6] = p ? *p : 0;
                s.f[7] = *reinterpret_cast<const int32_t*>(rec + 0x4b4);
                s.f[8] = *reinterpret_cast<const int32_t*>(rec + 0x4ac);
                for (int k = 0; k < kStates; ++k) s.state[k] = *reinterpret_cast<const int32_t*>(rec + kStateOffsets[k]);
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

        // Sets g_playDead[slot] while a player lies in Play Dead (skill 360), for the colour routine. Other players'
        // records on this PC never show the lying pose itself, so it is tracked from what they do show:
        //   on:  their current action (status +0x4b4) becomes 360, or their skill-use counter (+0x4a4) moves on while
        //        it stays 360 (Play Dead used again)
        //   off: they move (position +0xa0/+0xa4/+0xa8 leaves the spot where they lay down, after the fall), take damage
        //        (health +0x0c drops; Play Dead's own regeneration raises it), die, or use another skill.
        void TrackPlayDead()
        {
            std::thread([]
            {
                constexpr int32_t kPlayDead = 360;
                constexpr DWORD kFallMs = 1000;    // the lie-down animation may still move them a little
                constexpr float kMoved = 0.75f;    // world units from the spot they lay down on
                constexpr size_t kInts = 0x4b8 / 4;
                struct Track { bool seen = false, on = false, based = false; int32_t action = 0, counter = 0, hp = 0; float pos[3]{}; DWORD since = 0; };
                Track tracks[4];
                std::vector<int32_t> rec(kInts);
                auto stop = [](int slot, Track& t, const wchar_t* why)
                {
                    t.on = false;
                    g_playDead[slot] = 0;
                    Log(L"gamepatch: P%d stopped playing dead (%s)", slot + 1, why);
                };
                for (;;)
                {
                    Sleep(20);
                    for (int slot = 0; slot < 4; ++slot)
                    {
                        Track& t = tracks[slot];
                        auto base = *reinterpret_cast<uint8_t**>(At(0x1404bc608) + slot * 8);
                        if (!base || !CopyMemory32(base, rec.data(), kInts))
                        {
                            if (t.on) stop(slot, t, L"left the battle");
                            t = Track{};
                            continue;
                        }
                        int32_t hp = rec[0x0c / 4], action = rec[0x4b4 / 4], counter = rec[0x4a4 / 4];
                        float pos[3];
                        memcpy(pos, &rec[0xa0 / 4], sizeof(pos));
                        bool used = t.seen && action == kPlayDead && (t.action != kPlayDead || counter != t.counter);
                        if (t.on)
                        {
                            float d2 = 0;
                            for (int k = 0; k < 3; ++k) d2 += (pos[k] - t.pos[k]) * (pos[k] - t.pos[k]);
                            if (hp <= 0) stop(slot, t, L"died");
                            else if (action != kPlayDead || counter != t.counter) stop(slot, t, L"used another skill");
                            else if (hp < t.hp) stop(slot, t, L"took damage");
                            else if (t.based && d2 > kMoved * kMoved) stop(slot, t, L"moved");
                            else
                            {
                                t.hp = hp; // regeneration while lying
                                if (!t.based && GetTickCount() - t.since >= kFallMs)
                                {
                                    memcpy(t.pos, pos, sizeof(pos));
                                    t.based = true;
                                }
                            }
                        }
                        if (used && hp > 0)
                        {
                            t.on = true;
                            t.based = false;
                            t.hp = hp;
                            t.since = GetTickCount();
                            g_playDead[slot] = 1;
                            Log(L"gamepatch: P%d is playing dead", slot + 1);
                        }
                        t.seen = true;
                        t.action = action;
                        t.counter = counter;
                    }
                }
            }).detach();
        }

        // nametag_debug=deep: also every int32 of each player's object (DAT_1404bc9d8[i], 0x14000 bytes; it holds the
        // status record at +0x40) that changes rarely: each offset is logged for its first 12 changes, so constantly
        // changing values (positions, timers) drop out within a second.
        void DeepDebug()
        {
            std::thread([]
            {
                struct Area { int player; uint64_t table; size_t count; std::vector<int32_t> last; std::vector<uint8_t> changes; bool have = false; };
                Area areas[] = { { 1, 0x1404bc9d8, 0x14000 / 4 }, { 2, 0x1404bc9e0, 0x14000 / 4 }, { 3, 0x1404bc9e8, 0x14000 / 4 }, { 4, 0x1404bc9f0, 0x14000 / 4 } };
                Log(L"nametag-debug: deep mode, logging rare changes in each player's object");
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
                            if (a.changes[k] < 12)
                            {
                                ++a.changes[k];
                                Log(L"nametag-debug: P%d object+0x%X %d -> %d (0x%X)", a.player, static_cast<unsigned>(k * 4), a.last[k], now[k], now[k]);
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
                        wchar_t states[256] = {};
                        for (int k = 0; k < kStates; ++k)
                            swprintf_s(states + wcslen(states), _countof(states) - wcslen(states), L" %X=%d", kStateOffsets[k], s.state[k]);
                        char name[0x41] = {};
                        memcpy(name, At(0x1404bca30) + i * 0x41, 0x40);
                        Log(L"nametag-debug: P%d '%S' hp=%d 54=%d 58=0x%X 5c=%d 60=%d f8=%s%d action=%d phase=%d effects:%s |%s", i + 1, name, s.f[0], s.f[1], s.f[2], s.f[3], s.f[4],
                            s.f[5] ? L"*" : L"null ", s.f[6], s.f[7], s.f[8], effects[0] ? effects : L" none", states);
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
            if (NametagsOnBodies())
            {
                DeadNametagColor(GetConfig().deadNametagColor);
                TrackPlayDead();
            }
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
