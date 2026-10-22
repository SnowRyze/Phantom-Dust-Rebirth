#include "pch.h"
#include "gamepatches.h"
#include "config.h"
#include "lobbies.h"
#include "log.h"
#include <set>
#include <tuple>

namespace pdnet::gamepatches
{
    namespace
    {
        constexpr uint64_t kGhidraBase = 0x140000000;

        std::atomic<uint8_t> g_bodyTags{ 0 }; // nametags on dead bodies wanted (the lobby host's setting)
        // What the game's nametag code reads (routines below), kept current by NametagColours: dead bodies keep their
        // tag only in Tag Team, and each other player's tag colour by match slot (ally / enemy, both ARGB twice).
        std::atomic<uint8_t> g_bodyTagsNow{ 0 };
        std::atomic<uint64_t> g_tagColour[8]{ ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull };
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
        // `argb` instead, while dead bodies are tagged (g_bodyTagsNow: the lobby host's setting, in Tag Team), for a
        // player who is dead (health +0x0c of DAT_1404bc608[r15], the record the game's health check uses) or playing
        // dead (g_playDead[slot], set by TrackPlayDead, for r15 or the drawn slot rbx; the two differ in some battle
        // types). A fake death then looks like a real one. Anyone else gets their slot's ally / enemy colour
        // (g_tagColour[r15], NametagColours).
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
            std::vector<size_t> toNormal;
            code.insert(code.end(), { 0x48, 0xb8 });                 // mov rax, &g_bodyTagsNow
            Put64(code, reinterpret_cast<uint64_t>(&g_bodyTagsNow));
            code.insert(code.end(), { 0x80, 0x38, 0x00 });           // cmp byte [rax], 0
            jump(kJe, toNormal);
            code.insert(code.end(), { 0x48, 0x8d, 0x05 });           // lea rax, [DAT_1404bc608]
            Put32(code, Rel32(rip(4), At(0x1404bc608)));
            code.insert(code.end(), { 0x4a, 0x8b, 0x04, 0xf8 });     // mov rax, [rax + r15*8]
            code.insert(code.end(), { 0x48, 0x85, 0xc0 });           // test rax, rax
            jump(kJe, toNormal);
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
            jump(kJa, toNormal);
            code.insert(code.end(), { 0x80, 0x3c, 0x18, 0x00 });     // cmp byte [rax + rbx], 0
            jump(kJne, toDead);
            jump(kJmp, toNormal);
            label(toDead);
            code.insert(code.end(), { 0x48, 0xb8 });                 // dead: mov rax, colour
            Put64(code, (static_cast<uint64_t>(argb) << 32) | argb);
            std::vector<size_t> toStore;
            jump(kJmp, toStore);
            label(toNormal);                                         // alive: the slot's ally / enemy colour
            code.insert(code.end(), { 0x49, 0x83, 0xff, 0x07 });     // cmp r15, 7
            jump(kJa, toWhite);
            code.insert(code.end(), { 0x48, 0xb8 });                 // mov rax, &g_tagColour
            Put64(code, reinterpret_cast<uint64_t>(&g_tagColour[0]));
            code.insert(code.end(), { 0x4a, 0x8b, 0x04, 0xf8 });     // mov rax, [rax + r15*8]
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
        // Those become a jump to a routine that skips the health check while g_bodyTagsNow is 1, so the name stays on a
        // player at 0 health (a dead body). It is 1 only in Tag Team with the lobby host's setting on (NametagColours).
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
            code.insert(code.end(), { 0x50, 0x48, 0xb8 });                  // push rax ; mov rax, &g_bodyTagsNow
            Put64(code, reinterpret_cast<uint64_t>(&g_bodyTagsNow));
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

        // Nametag colours. Three kinds: enemy (every game type), ally (Tag Team only: same team as this PC's player) and
        // dead body (Tag Team only, the existing dead colour). Read from the game's session data:
        //   game type 0x1405277b4 (0 Battle Royale, 1 Tag Team, 2 One-on-One; session command 55),
        //   this PC's lobby slot 0x140527bc4, each lobby slot's team 0x140527c40[slot] (session command 134),
        //   lobby names 0x140527ac0 + slot*64; the nametag code's players (match slot r15) are named at
        //   0x1404bca30 + i*0x41, so each one is matched to its lobby slot by name.
        struct TeamView
        {
            int32_t type, mySlot, team[4];
            char lobbyName[4][64];
            char matchName[4][0x41];
        };

        bool ReadTeamView(TeamView& v)
        {
            __try
            {
                v.type = *reinterpret_cast<int32_t*>(At(0x1405277b4));
                v.mySlot = *reinterpret_cast<int32_t*>(At(0x140527bc4));
                memcpy(v.team, At(0x140527c40), sizeof(v.team));
                for (int i = 0; i < 4; ++i)
                {
                    memcpy(v.lobbyName[i], At(0x140527ac0) + i * 64, 64);
                    v.lobbyName[i][63] = 0;
                    memcpy(v.matchName[i], At(0x1404bca30) + i * 0x41, 0x41);
                    v.matchName[i][0x40] = 0;
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        void NametagColours()
        {
            auto twice = [](uint32_t argb) { return static_cast<uint64_t>(argb) << 32 | argb; };
            bool colours = GetConfig().nametagColors; // nametag_colors=off: the game's white for everyone
            uint64_t ally = twice(colours ? GetConfig().allyNametagColor : 0xffffffff), enemy = twice(colours ? GetConfig().enemyNametagColor : 0xffffffff);
            std::thread([ally, enemy, twice]
            {
                std::wstring last;
                for (;;)
                {
                    Sleep(100);
                    TeamView v{};
                    if (!ReadTeamView(v)) continue;
                    bool tag = v.type == 1;
                    g_bodyTagsNow = g_bodyTags && tag ? 1 : 0;
                    int myTeam = v.mySlot >= 0 && v.mySlot < 4 ? v.team[v.mySlot] : -1;
                    std::wstring summary = tag ? L"Tag Team" : L"no teams";
                    for (int i = 0; i < 4; ++i)
                    {
                        uint64_t colour = enemy;
                        const wchar_t* kind = L"enemy";
                        if (!v.matchName[i][0])
                        {
                            colour = twice(0xffffffff);
                            kind = L"-";
                        }
                        else if (tag)
                        {
                            int team = -1;
                            for (int s = 0; s < 4; ++s)
                                if (strcmp(v.lobbyName[s], v.matchName[i]) == 0) team = v.team[s];
                            if (team < 0 || myTeam < 0)
                            {
                                colour = twice(0xffffffff); // team unknown: the game's own white
                                kind = L"unknown";
                            }
                            else if (team == myTeam)
                            {
                                colour = ally;
                                kind = L"ally";
                            }
                        }
                        g_tagColour[i] = colour;
                        summary += L", " + std::to_wstring(i) + L" " + kind;
                    }
                    if (summary != last)
                    {
                        Log(L"gamepatch: nametag colours: %s (dead bodies %s)", summary.c_str(), g_bodyTagsNow ? L"tagged" : L"not tagged");
                        last = summary;
                    }
                }
            }).detach();
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

    namespace
    {
        // The game's menus are scripts on a stack (manager = DAT_140533c58, current menu index at +0x105f4):
        //   FUN_140246130(manager, menu, force) opens `menu` on top (saves the current one and its state),
        //   FUN_1402461f0() closes the top one and re-opens the saved one.
        // Menu indexes (table DAT_1403fa3d0, 0x18 per entry): livelobby 44, laboratory 45, labostock 63, arsenaledit 64.
        // Lobby Arsenal editor: the lobby's invite button (pdnet's ShowGameInviteUIAsync) opens arsenaledit on top
        // of livelobby; while it is open from there, its exit to another menu (normally the Laboratory) becomes a
        // "back", so it returns to the lobby with the lobby's state restored, like the lobby's own Change Arsenal.
        constexpr int kLobbyMenu = 44, kStockMenu = 63, kEditorMenu = 64;
        using PushMenuFn = int (*)(int64_t manager, int menu, char force);
        using PopMenuFn = int (*)();
        PushMenuFn g_pushMenu = nullptr; // the original functions (trampolines)
        PopMenuFn g_popMenu = nullptr;
        std::atomic<bool> g_editorFromLobby{ false };

        int64_t MenuManager() { return *reinterpret_cast<int64_t*>(At(0x140533c58)); }
        int CurrentMenu(int64_t manager) { return manager ? *reinterpret_cast<int16_t*>(manager + 0x105f4) : -1; }

        // The button labels come from Assets\Data\newstrings.txt, which the game parses once into a table
        // (DAT_1405b0918: +0 keys, +8 values in the player's language, +0x10 count). The lobby's help lines
        // (lobby_normal / lobby_team / lobby_locked_out) say "\a5#Invite"; their value pointers are pointed at copies
        // that say "Arsenal Lab". Nothing on disk changes, so this works in the Store version too.
        struct StringTable { char** keys; char** values; int32_t count; };

        bool ReadStringTable(StringTable& t)
        {
            __try
            {
                auto table = *reinterpret_cast<uint8_t**>(At(0x1405b0918));
                if (!table) return false;
                t = { *reinterpret_cast<char***>(table), *reinterpret_cast<char***>(table + 8), *reinterpret_cast<int32_t*>(table + 0x10) };
                return t.keys && t.values && t.count > 0 && t.count < 100000;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        bool CopyString(const char* from, char* to, size_t size)
        {
            __try
            {
                size_t n = 0;
                for (; n + 1 < size && from[n]; ++n) to[n] = from[n];
                to[n] = 0;
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // Text replacements in the string table, matched on key and on the English text (other languages are left).
        struct Rename { const char* key; const char* find; const char* replace; };
        constexpr Rename kRenames[] = {
            { "lobby_normal", "#Invite", "#Arsenal Lab" },
            { "lobby_team", "#Invite", "#Arsenal Lab" },
            { "lobby_locked_out", "#Invite", "#Arsenal Lab" },
            // The Help & Support button (the Phantom Dust page on xbox.com) imports the Xbox save (gamesave.cpp).
            { "help_main", "HELP & SUPPORT", "IMPORT XBOX SAVE" },
            { "help", "Help & Support", "Import Xbox Save" },
            { "help_hint", "Go to the Phantom Dust page on Xbox.com.", "Copy your Xbox Live save (progress, arsenals, play time) into this game. Restart the game afterwards." },
        };

        void RenameStrings()
        {
            static std::mutex lock;
            static std::map<std::string, std::string> renamed; // stable storage for the new values
            std::lock_guard guard(lock);
            StringTable t{};
            if (!ReadStringTable(t)) return;
            for (int i = 0; i < t.count; ++i)
            {
                char key[64], value[512];
                if (!CopyString(t.keys[i], key, sizeof(key))) return;
                // icon_gallery=on: the main menu's hint line (Import Xbox Save highlighted) shows every inline icon
                // (text code 0x07 N '#', icons 0-36) with its number, to pick one for voice_talk_mark.
                // In the lobby (its own sprite sheet), the bottom hint line shows icons 19-36 instead.
                if (GetConfig().iconGallery && strcmp(key, "help_hint") == 0)
                {
                    static std::string gallery;
                    if (gallery.empty())
                        for (int n = 0; n <= 36; ++n)
                            gallery += "\a" + std::to_string(n) + "#" + std::to_string(n) + (n % 10 == 9 ? "\n" : "  ");
                    if (t.values[i] != gallery.data()) Log(L"gamepatch: icon gallery in the main menu's hint line");
                    t.values[i] = gallery.data();
                    continue;
                }
                if (GetConfig().iconGallery && (strcmp(key, "lobby_normal") == 0 || strcmp(key, "lobby_team") == 0 || strcmp(key, "lobby_locked_out") == 0))
                {
                    static std::string lobbyGallery;
                    if (lobbyGallery.empty())
                        for (int n = 19; n <= 36; ++n) lobbyGallery += "\a" + std::to_string(n) + "#" + std::to_string(n) + " ";
                    if (t.values[i] != lobbyGallery.data()) Log(L"gamepatch: icon gallery (19-36) in the lobby's hint line '%S'", key);
                    t.values[i] = lobbyGallery.data();
                    continue;
                }
                for (auto& r : kRenames)
                {
                    if (strcmp(key, r.key) != 0) continue;
                    if (!CopyString(t.values[i], value, sizeof(value))) return;
                    std::string text = value;
                    size_t at = text.find(r.find);
                    if (at == std::string::npos) break; // already renamed, or another language
                    text.replace(at, strlen(r.find), r.replace);
                    std::string& stored = renamed[key];
                    stored = text;
                    t.values[i] = stored.data();
                    Log(L"gamepatch: text '%S' now says %S", key, r.replace);
                    break;
                }
            }
        }

        int HookedPushMenu(int64_t manager, int menu, char force)
        {
            int current = CurrentMenu(manager);
            RenameStrings(); // cheap; covers string tables loaded after start-up
            if (menu != current) Log(L"menu: open %d (from %d)", menu, current);
            if (g_editorFromLobby && current == kEditorMenu && menu != kEditorMenu && menu != kStockMenu)
            {
                Log(L"menu: leaving the Arsenal editor opened from the lobby - back to the lobby instead of menu %d", menu);
                g_editorFromLobby = false;
                return g_popMenu();
            }
            return g_pushMenu(manager, menu, force);
        }

        int HookedPopMenu()
        {
            int current = CurrentMenu(MenuManager());
            Log(L"menu: back (from %d)", current);
            if (current == kEditorMenu) g_editorFromLobby = false;
            return g_popMenu();
        }

        // Replaces the 10-byte prologue `mov [rsp+8],rbx / push rdi / sub rsp,0x20` at `ghidra` with a jump to `hook`
        // (through a 14-byte absolute jump in the code cave) and returns a trampoline that runs the prologue and
        // continues in the original function.
        void* InlineHook(uint64_t ghidra, void* hook, const wchar_t* name)
        {
            const uint8_t prologue[10] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20 };
            uint8_t* site = At(ghidra);
            if (memcmp(site, prologue, sizeof(prologue)) != 0)
            {
                Log(L"gamepatch: %s hook skipped - the game code is not what this pdnet expects", name);
                return nullptr;
            }
            auto trampoline = static_cast<uint8_t*>(VirtualAlloc(nullptr, 32, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            uint8_t* cave = CodeCave(16);
            if (!trampoline || !cave)
            {
                Log(L"gamepatch: %s hook skipped - no memory for it", name);
                return nullptr;
            }
            std::vector<uint8_t> back(prologue, prologue + sizeof(prologue));
            back.insert(back.end(), { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 });     // jmp [rip+0]
            Put64(back, reinterpret_cast<uint64_t>(site + sizeof(prologue)));
            memcpy(trampoline, back.data(), back.size());
            FlushInstructionCache(GetCurrentProcess(), trampoline, back.size());

            std::vector<uint8_t> toHook = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 }; // jmp [rip+0]
            Put64(toHook, reinterpret_cast<uint64_t>(hook));
            std::vector<uint8_t> jump = { 0xe9 };
            Put32(jump, Rel32(site + 5, cave));
            jump.resize(sizeof(prologue), 0x90);
            if (!Write(cave, toHook) || !Write(site, jump))
            {
                Log(L"gamepatch: %s hook failed (VirtualProtect error %lu)", name, GetLastError());
                return nullptr;
            }
            return trampoline;
        }

        // menu_debug=on: while the lobby menu (44) is on screen, logs each script command (menu table 0x1403eefe0 x136,
        // session table 0x1403f1150 x196) the first time it's called and whenever its result or first argument
        // changes - to see what the lobby script checks (e.g. what greys out an option).

        void LogLobbyCommand(uint32_t id, int64_t result, const int32_t* args)
        {
            static std::atomic<int> logged{ 0 };
            uint32_t table = id >> 16, index = id & 0xffff;
            if (table > 1 || index > 255 || CurrentMenu(MenuManager()) != kLobbyMenu || logged > 5000) return;
            int32_t arg = 0;
            if (args && !IsBadReadPtr(args, 4)) arg = args[0];
            // Each command + argument + result once: some are called every frame with alternating arguments.
            static std::mutex lock;
            static std::set<std::tuple<uint32_t, int32_t, int64_t>> seen;
            {
                std::lock_guard guard(lock);
                if (!seen.emplace(id, arg, result).second) return;
            }
            ++logged;
            Log(L"menu-debug: %s cmd %u(arg %d) -> %lld", table ? L"session" : L"menu", index, arg, result);
        }

        void MenuDebug()
        {
            const struct { uint64_t address; int count; } tables[2] = { { 0x1403eefe0, 136 }, { 0x1403f1150, 196 } };
            for (uint32_t t = 0; t < 2; ++t)
            {
                int count = tables[t].count;
                auto stubs = static_cast<uint8_t*>(VirtualAlloc(nullptr, count * 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
                auto table = reinterpret_cast<uint64_t*>(At(tables[t].address));
                DWORD old = 0;
                if (!stubs || !VirtualProtect(table, count * 8, PAGE_READWRITE, &old)) return;
                for (int i = 0; i < count; ++i)
                {
                    std::vector<uint8_t> s = { 0x48, 0x83, 0xec, 0x38,          // sub rsp, 0x38
                                               0x48, 0x89, 0x4c, 0x24, 0x20,    // mov [rsp+0x20], rcx (the script's arguments)
                                               0x48, 0xb8 };                    // mov rax, original
                    Put64(s, table[i]);
                    s.insert(s.end(), { 0xff, 0xd0,                             // call rax
                                        0x48, 0x89, 0x44, 0x24, 0x28,           // mov [rsp+0x28], rax
                                        0xb9 });                                // mov ecx, table << 16 | index
                    Put32(s, static_cast<int32_t>(t << 16 | i));
                    s.insert(s.end(), { 0x48, 0x89, 0xc2,                       // mov rdx, rax
                                        0x4c, 0x8b, 0x44, 0x24, 0x20,           // mov r8, [rsp+0x20]
                                        0x48, 0xb8 });                          // mov rax, LogLobbyCommand
                    Put64(s, reinterpret_cast<uint64_t>(&LogLobbyCommand));
                    s.insert(s.end(), { 0xff, 0xd0,                             // call rax
                                        0x48, 0x8b, 0x44, 0x24, 0x28,           // mov rax, [rsp+0x28]
                                        0x48, 0x83, 0xc4, 0x38,                 // add rsp, 0x38
                                        0xc3 });                                // ret
                    memcpy(stubs + i * 64, s.data(), s.size());
                    table[i] = reinterpret_cast<uint64_t>(stubs + i * 64);
                }
                VirtualProtect(table, count * 8, old, &old);
                FlushInstructionCache(GetCurrentProcess(), stubs, count * 64);
            }
            Log(L"gamepatch: menu_debug - logging the lobby script's commands");
        }

        // Lobby Gamertag option. livelobby.ssb greys out Options > Gamertag (and Kick Player) unless its
        // player-count function says 2 or more, and the select handlers ignore the button below that. The script is
        // loaded fresh each time the lobby opens (menu table entry +0x10, filled once the file has loaded), so a
        // watcher patches each new copy in memory: Gamertag's draw check `count < 2` becomes `count < 0`, and its
        // host and client select checks `count > 1` become `count > -1`. Kick Player keeps its own checks. Every site
        // is matched on the exact bytecode first (any language's livelobby), so another game version is left alone.
        // Script words: op = low byte; 0x03 call label (target = next word + imm*4), 0x0E push imm, 0x10 push result,
        // 0x11 push string (pool offset), 0x17 compare, 0x07 jump.
        constexpr uint32_t kPush2 = 0x0002000E, kPush1 = 0x0001000E, kPushResult = 0x00000010;

        const uint32_t* LabelTarget(const uint32_t* word)
        {
            return (*word & 0xffff) == 0x0003 ? word + 1 + static_cast<int16_t>(*word >> 16) : nullptr;
        }

        // Lobby Mic label (voice_chat=on): the option that says "Voice Chat: On/Off" now switches only this PC's mic
        // (pdnet voice, see lobbies.h), so its two strings in the script's pool are overwritten in place (shorter).
        struct Relabel { const char* find; const char* replace; };
        constexpr Relabel kLobbyLabels[] = { { "Voice Chat: Off", "Mic: Off" }, { "Voice Chat: On", "Mic: On" } };

        // Returns how many Gamertag checks were patched (-1: not a script this pdnet understands); `labels` counts
        // the relabelled strings.
        int PatchLobbyScript(uint8_t* script, bool gamertag, bool micLabel, int& labels)
        {
            labels = 0;
            __try
            {
                // Header: +0 code offset (0x20), +4 labels, +8 end of labels, +12 strings, as file offsets. Once
                // loaded (FUN_1402aa620) +0 is reused by the game (e.g. 0x2FF) and the other three are relative to
                // their own field.
                auto code = reinterpret_cast<uint32_t*>(script);
                constexpr uint32_t codeStart = 0x20;
                uint32_t labels = code[1], strings = code[3];
                if (code[0] != codeStart) labels += 4, strings += 12;
                if (labels <= codeStart || labels > 0x40000 || strings < labels || strings > 0x80000) return -1;
                uint32_t* end = code + labels / 4;
                const char* pool = reinterpret_cast<const char*>(script + strings);
                // The Gamertag window: `drop 2; push 1; push 5; open window` (window 5 = a player list, which Kick
                // Player opens too), titled "Gamertag" 10 words on.
                auto opensGamertag = [pool](const uint32_t* f)
                {
                    return f && f[0] == 0x00020006 && f[1] == 0x0001000E && f[2] == 0x0005000E && f[3] == 0x20610201 &&
                           (f[10] & 0xffff) == 0x0011 && strcmp(pool + (f[10] >> 16), "Gamertag") == 0;
                };
                int patched = 0;
                for (uint32_t* w = code + codeStart / 4; w + 40 < end; ++w)
                {
                    if (micLabel && (*w & 0xffff) == 0x0011)
                    {
                        char* text = const_cast<char*>(pool + (*w >> 16));
                        for (auto& r : kLobbyLabels)
                        {
                            if (strcmp(text, r.find) == 0) memcpy(text, r.replace, strlen(r.replace) + 1);
                            else if (strcmp(text, r.replace) != 0) continue;
                            ++labels; // relabelled now, or already (the loaded copy can keep an earlier patch)
                        }
                    }
                    if (!gamertag || !LabelTarget(w) || w[1] != kPushResult) continue;
                    // Draw: count; push 2; less-than; jump-if-zero +0x1C; ... push "$GAMERTAG" 0x98 bytes on.
                    if (w[2] == kPush2 && w[3] == 0x00030017 && w[4] == 0x001C0207 && (w[38] & 0xffff) == 0x0011 &&
                        strcmp(pool + (w[38] >> 16), "$GAMERTAG") == 0)
                    {
                        w[2] = 0x0000000E;
                        ++patched;
                    }
                    // Select: count; push 1; greater-than; jump-if-zero +1; call the Gamertag window.
                    else if (w[2] == kPush1 && w[3] == 0x00020017 && w[4] == 0x00010207 && opensGamertag(LabelTarget(w + 5)))
                    {
                        w[2] = 0xFFFF000E;
                        ++patched;
                    }
                }
                return patched;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1;
            }
        }

        void LobbyScriptPatches(bool gamertag, bool micLabel)
        {
            // Each load is patched once; the entry goes back to null when the lobby closes (QuitMenu frees the
            // script), which also covers the next load reusing the same buffer.
            std::thread([gamertag, micLabel]
            {
                auto entry = reinterpret_cast<uint8_t* volatile*>(At(0x1403fa3d0) + kLobbyMenu * 0x18 + 0x10);
                uint8_t* done = nullptr;
                for (;;)
                {
                    Sleep(50);
                    uint8_t* script = *entry;
                    if (script == done) continue;
                    done = script;
                    if (!script) continue;
                    int labels = 0;
                    int n = PatchLobbyScript(script, gamertag, micLabel, labels);
                    if (gamertag)
                    {
                        if (n == 3) Log(L"gamepatch: lobby Gamertag option enabled (3 script checks)");
                        else Log(L"gamepatch: lobby Gamertag option - %d of 3 script checks matched (script at %p)", n, script);
                    }
                    if (micLabel) Log(L"gamepatch: lobby Voice Chat option relabelled Mic (%d of 2 texts)", labels);
                }
            }).detach();
            Log(L"gamepatch: lobby script watcher started (Gamertag option %s, Mic label %s)", gamertag ? L"on" : L"off", micLabel ? L"on" : L"off");
        }

        // Lobby talking indicator (voice_chat=on), as the original Xbox game had: while a player speaks in the voice
        // room, their name in the lobby's player list blinks a mark after it. The list gets each name from session
        // command 23 (0x1402139d0: a pointer to the slot's name, lobby names at 0x140527ac0 + slot*64); its table
        // entry is wrapped so that, in the lobby, a talking player's name comes back with the mark added.
        // voice_talk_mark: plain UTF-8 text, or #N for the game's inline icon N (text code 0x07 N '#').
        using SessionCommandFn = int64_t (*)(int32_t* args);
        SessionCommandFn g_playerName = nullptr;
        std::string g_talkMark;

        // The lobby's player rows also draw a status lamp per slot with menu command 95 (0x1402077a0: sprite from a
        // loaded texture slot; args sheet, sprite, x, y, colour, 8 bytes each): sprite 34 or 35 of MNET (texture slot 3,
        // loaded by livelobbyres; 84 sprites) right after session command 26 for that row's slot.
        //   voice_talk_icon=N: MNET sprite N (the original game's megaphone) after the name of each player whose mic is
        //   on, in the name's colour (HookedDrawText); icon_gallery=on: this PC's own lamp steps through MNET sprites 0-83 (1.5 s each) and its name
        //   shows the number, to find a sprite.
        using MenuCommandFn = int64_t (*)(int64_t* args);
        MenuCommandFn g_drawSprite = nullptr;
        SessionCommandFn g_slotStatus = nullptr;
        std::atomic<int> g_rowSlot{ -1 };
        constexpr int kMnetSprites = 84;

        int GallerySprite() { return static_cast<int>(GetTickCount64() / 1500 % kMnetSprites); }

        // The megaphone goes right after the player's name. The row draws its lamp first (its y is kept for the
        // megaphone), then the name with menu command 23 (0x140207840; args x, y, format, name, ...): format
        // "\a20s\a105s\a190w%s" (Tag Team) or "\a12s\a98s\a190w%s\a370r%d" - the name starts 125 / 110 px right of x
        // and is clipped at 190 px. Its width comes from the game's own text measure (FUN_140247550, as used for
        // right-aligned text).
        MenuCommandFn g_drawText = nullptr;
        std::atomic<int64_t> g_lampY{ 0 };
        using MeasureFn = int (*)(const char* text, uint32_t font, int mode);

        int NameIndent(const char* format)
        {
            char f[64];
            if (!CopyString(format, f, sizeof(f)) || !strstr(f, "190w%s")) return -1;
            if (strstr(f, "20s\a105s")) return 125;
            if (strstr(f, "12s\a98s")) return 110;
            return -1;
        }

        int64_t HookedDrawText(int64_t* args)
        {
            int64_t result = g_drawText(args);
            int icon = GetConfig().voiceTalkIcon;
            if (!args || icon < 0 || icon >= kMnetSprites || !g_drawSprite || CurrentMenu(MenuManager()) != kLobbyMenu) return result;
            int indent = NameIndent(reinterpret_cast<const char*>(args[2]));
            int slot = g_rowSlot;
            if (indent < 0 || slot < 0 || slot >= 4) return result;
            char name[64];
            if (!CopyString(reinterpret_cast<const char*>(args[3]), name, sizeof(name)) || !name[0]) return result;
            if (!GetConfig().voiceDebug && !lobbies::IsMicOn(name)) return result; // voice_debug: always, to see where it lands
            int width = (std::min)(reinterpret_cast<MeasureFn>(At(0x140247550))(name, 0, 0), 190);
            // In the colour the name was just drawn in: the game's current text colour (ARGB, 0x14054c9b0).
            uint32_t colour = *reinterpret_cast<uint32_t*>(At(0x14054c9b0));
            int64_t megaphone[5] = { 3, icon, args[0] + indent + width + GetConfig().voiceTalkIconX, g_lampY, static_cast<int64_t>(colour) };
            g_drawSprite(megaphone);
            return result;
        }

        int64_t HookedSlotStatus(int32_t* args)
        {
            if (args) g_rowSlot = args[0];
            return g_slotStatus(args);
        }

        int64_t HookedDrawSprite(int64_t* args)
        {
            if (!args || args[0] != 3 || (args[1] != 34 && args[1] != 35) || CurrentMenu(MenuManager()) != kLobbyMenu) return g_drawSprite(args);
            int slot = g_rowSlot;
            int mine = *reinterpret_cast<int32_t*>(At(0x140527bc4));
            if (GetConfig().iconGallery && slot == mine)
            {
                int64_t gallery[5] = { 3, GallerySprite(), args[2], args[3], static_cast<int64_t>(0xffffffff) };
                return g_drawSprite(gallery);
            }
            g_lampY = args[3]; // the megaphone is drawn at this height, after the name (HookedDrawText)
            if (GetConfig().voiceDebug && slot >= 0 && slot < 4)
            {
                static uint64_t next[4] = {};
                if (GetTickCount64() >= next[slot])
                {
                    char name[64];
                    bool named = CopyString(reinterpret_cast<const char*>(At(0x140527ac0) + slot * 64), name, sizeof(name)) && name[0];
                    Log(L"voice-debug: lobby row slot %d '%S' talking %d, lamp at %lld,%lld", slot, named ? name : "", named && lobbies::IsTalking(name) ? 1 : 0, args[2], args[3]);
                    next[slot] = GetTickCount64() + 3000;
                }
            }
            return g_drawSprite(args);
        }

        int64_t HookedPlayerName(int32_t* args)
        {
            int64_t result = g_playerName(args);
            int slot = args ? args[0] : -1;
            if (!result || slot < 0 || slot >= 8 || CurrentMenu(MenuManager()) != kLobbyMenu) return result;
            static char named[8][160];
            if (GetConfig().iconGallery && slot == *reinterpret_cast<int32_t*>(At(0x140527bc4)))
            {
                if (!CopyString(reinterpret_cast<const char*>(result), named[slot], 64)) return result;
                strcat_s(named[slot], (" [" + std::to_string(GallerySprite()) + "]").c_str());
                return reinterpret_cast<int64_t>(named[slot]);
            }
            if (GetConfig().voiceTalkIcon >= 0) return result; // the megaphone sprite shows talking instead of a text mark
            if ((GetTickCount64() / 300) % 2) return result; // blink: the mark shows 300 ms, hides 300 ms
            if (!CopyString(reinterpret_cast<const char*>(result), named[slot], 64) || !named[slot][0] || !lobbies::IsTalking(named[slot])) return result;
            strcat_s(named[slot], " ");
            strcat_s(named[slot], g_talkMark.c_str());
            return reinterpret_cast<int64_t>(named[slot]);
        }

        template <typename Fn>
        bool WrapCommand(uint64_t tableEntry, uint64_t expected, Fn hook, Fn& original, const wchar_t* name)
        {
            auto entry = reinterpret_cast<Fn*>(At(tableEntry));
            DWORD old = 0;
            if (*entry != reinterpret_cast<Fn>(At(expected)))
            {
                Log(L"gamepatch: %s skipped - the game code is not what this pdnet expects", name);
                return false;
            }
            if (!VirtualProtect(entry, 8, PAGE_READWRITE, &old)) return false;
            original = *entry;
            *entry = hook;
            VirtualProtect(entry, 8, old, &old);
            return true;
        }

        void LobbyTalkIndicator()
        {
            if (WrapCommand<MenuCommandFn>(0x1403eefe0 + 95 * 8, 0x1402077a0, &HookedDrawSprite, g_drawSprite, L"lobby sprite hook") &&
                WrapCommand<SessionCommandFn>(0x1403f1150 + 26 * 8, 0x140213a30, &HookedSlotStatus, g_slotStatus, L"lobby row hook") &&
                WrapCommand<MenuCommandFn>(0x1403eefe0 + 23 * 8, 0x140207840, &HookedDrawText, g_drawText, L"lobby name hook"))
                Log(L"gamepatch: lobby talking icon %s%s", GetConfig().voiceTalkIcon >= 0 ? (L"MNET sprite " + std::to_wstring(GetConfig().voiceTalkIcon)).c_str() : L"off (text mark)",
                    GetConfig().iconGallery ? L"; sprite gallery on this PC's lamp" : L"");
            const std::string& mark = GetConfig().voiceTalkMark;
            g_talkMark = mark.size() > 1 && mark[0] == '#' && isdigit(static_cast<unsigned char>(mark[1])) ? "\a" + mark.substr(1) + "#" : mark;
            auto entry = reinterpret_cast<SessionCommandFn*>(At(0x1403f1150) + 23 * 8);
            DWORD old = 0;
            if (*entry != reinterpret_cast<SessionCommandFn>(At(0x1402139d0)))
            {
                Log(L"gamepatch: lobby talking indicator skipped - the game code is not what this pdnet expects");
                return;
            }
            if (!VirtualProtect(entry, 8, PAGE_READWRITE, &old)) return;
            g_playerName = *entry;
            *entry = &HookedPlayerName;
            VirtualProtect(entry, 8, old, &old);
            Log(L"gamepatch: lobby talking indicator ready (mark '%S')", mark.c_str());
        }

        void LobbyArsenalEditor()
        {
            auto pop = reinterpret_cast<PopMenuFn>(InlineHook(0x1402461f0, reinterpret_cast<void*>(&HookedPopMenu), L"menu back"));
            if (!pop) return;
            g_popMenu = pop;
            auto push = reinterpret_cast<PushMenuFn>(InlineHook(0x140246130, reinterpret_cast<void*>(&HookedPushMenu), L"menu open"));
            if (!push) return;
            g_pushMenu = push;
            Log(L"gamepatch: lobby Arsenal editor ready (the lobby's invite button opens it)");
            RenameStrings(); // again whenever a menu opens, in case the strings were loaded later
            if (GetConfig().menuDebug) MenuDebug();
        }
    }

    bool OpenLobbyArsenalEditor()
    {
        int64_t manager = MenuManager();
        int current = CurrentMenu(manager);
        if (!g_pushMenu || current != kLobbyMenu) return false;
        Log(L"menu: invite button in the lobby - opening the Arsenal editor");
        g_editorFromLobby = true;
        *At(0x1404bbc34) = 0; // as the script's own "open menu" command does
        g_pushMenu(manager, kEditorMenu, 0);
        return true;
    }

    void Apply()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            g_bodyTags = GetConfig().nametagsOnBodies ? 1 : 0;
            if (NametagsOnBodies())
            {
                DeadNametagColor(GetConfig().nametagColors ? GetConfig().deadNametagColor : 0xffffffff);
                NametagColours();
                TrackPlayDead();
            }
            if (GetConfig().nametagDebug) NametagDebug();
            if (GetConfig().lobbyArsenalEditor) LobbyArsenalEditor();
            if (GetConfig().voiceChat || GetConfig().iconGallery) LobbyTalkIndicator();
            bool micLabel = GetConfig().voiceChat && !GetConfig().voiceClassic; // classic: the option keeps its "Voice Chat" name
            if (GetConfig().lobbyGamertagOption || micLabel) LobbyScriptPatches(GetConfig().lobbyGamertagOption, micLabel);
        });
    }

    // In a match or in the lobby: the game's state 0x1404bc950 is 0 or 13 in the lobby / menus (session command 23
    // then reads names from the lobby table), anything else in a match. Each change is logged once, to verify.
    bool TagTeamMatch(std::map<std::string, int>& teamOf, int& myTeam)
    {
        static int lastState = -999;
        int state = *reinterpret_cast<int32_t*>(At(0x1404bc950));
        if (state != lastState)
        {
            Log(L"gamepatch: game state %d (%s)", state, state == 0 || state == 13 ? L"lobby / menus" : L"match");
            lastState = state;
        }
        TeamView v{};
        if (state == 0 || state == 13 || !ReadTeamView(v) || v.type != 1 || v.mySlot < 0 || v.mySlot > 3) return false;
        myTeam = v.team[v.mySlot];
        teamOf.clear();
        for (int s = 0; s < 4; ++s)
            if (v.lobbyName[s][0]) teamOf[v.lobbyName[s]] = v.team[s];
        return true;
    }

    bool GameMicOn()
    {
        return *At(0x1404bad34) == 0; // settings flag 15 (script group 10 #3/#4): 1 = voice off
    }

    void SetNametagsOnBodies(bool on, const wchar_t* why)
    {
        uint8_t want = on ? 1 : 0;
        if (g_bodyTags.exchange(want) != want) Log(L"gamepatch: nametags on dead bodies %s (%s)", on ? L"on" : L"off", why);
    }

    void UseOwnSettings() { SetNametagsOnBodies(GetConfig().nametagsOnBodies, L"this PC's pdnet.ini"); }
}
