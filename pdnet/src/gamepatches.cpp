#include "pch.h"
#include "gamepatches.h"
#include "config.h"
#include "lobbies.h"
#include "scores.h"
#include "log.h"
#include <fstream>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <intrin.h>
#include <map>
#include <mutex>
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
        std::atomic<uint8_t> g_slotGone[4]{}; // per match slot: that player left (no nametag), set by NametagColours
        std::atomic<uint64_t> g_tagColour[8]{ ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull, ~0ull };
        std::atomic<uint64_t> g_deadColour{ ~0ull }; // a dead player's nametag colour, twice (read by the colour routine)
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

            g_deadColour = (static_cast<uint64_t>(argb) << 32) | argb;
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
            code.insert(code.end(), { 0x48, 0xb8 });                 // dead: mov rax, &g_deadColour
            Put64(code, reinterpret_cast<uint64_t>(&g_deadColour));
            code.insert(code.end(), { 0x48, 0x8b, 0x00 });           // mov rax, [rax]
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
            uint8_t* cave = CodeCave(96);
            if (!cave)
            {
                Log(L"gamepatch: nametags on dead bodies skipped - no free space for the check routine");
                return false;
            }
            std::vector<uint8_t> code;
            auto rip = [&](size_t instructionLength) { return cave + code.size() + instructionLength; };
            // A player who left the match (g_slotGone[r15]) has no nametag at all.
            code.insert(code.end(), { 0x50 });                              // push rax
            code.insert(code.end(), { 0x49, 0x83, 0xff, 0x03 });            // cmp r15, 3
            code.insert(code.end(), { 0x77, 0x00 });                        // ja present (patched below)
            size_t jaAt = code.size() - 1;
            code.insert(code.end(), { 0x48, 0xb8 });                        // mov rax, &g_slotGone
            Put64(code, reinterpret_cast<uint64_t>(&g_slotGone[0]));
            code.insert(code.end(), { 0x42, 0x80, 0x3c, 0x38, 0x00 });      // cmp byte [rax + r15], 0
            code.insert(code.end(), { 0x74, 0x00 });                        // je present (patched below)
            size_t jeAt = code.size() - 1;
            code.insert(code.end(), { 0x58, 0xe9 });                        // pop rax ; jmp skip (no nametag)
            Put32(code, Rel32(rip(4), skip));
            code[jaAt] = static_cast<uint8_t>(code.size() - (jaAt + 1));    // present:
            code[jeAt] = static_cast<uint8_t>(code.size() - (jeAt + 1));
            code.insert(code.end(), { 0x48, 0xb8 });                        // mov rax, &g_bodyTagsNow (rax still pushed)
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

        // Which match slots still have their player, as session command 28 (0x140213a80) decides it: this PC's own slot
        // (0x140527bc4), or an active entry for the slot in the game's peer list (count 0x140594ea4; 0x298-byte entries,
        // active flag at 0x140595ea8, slot at 0x140595eac). `state` is the game state (0 / 13: lobby or menus).
        bool ReadPresence(bool present[4], int32_t& state)
        {
            __try
            {
                state = *reinterpret_cast<int32_t*>(At(0x1404bc950));
                int32_t mine = *reinterpret_cast<int32_t*>(At(0x140527bc4));
                int32_t peers = *reinterpret_cast<int32_t*>(At(0x140594ea4));
                for (int s = 0; s < 4; ++s) present[s] = s == mine;
                for (int i = 0; i < peers && i < 16; ++i)
                {
                    uint8_t* entry = At(0x140595ea8) + i * 0x298;
                    int32_t active = *reinterpret_cast<int32_t*>(entry), slot = *reinterpret_cast<int32_t*>(entry + 4);
                    if (active && slot >= 0 && slot < 4) present[slot] = true;
                }
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        // During a match, a slot whose player left (named, but no longer present) gets no nametag (g_slotGone).
        void UpdateLeftPlayers(const TeamView& v)
        {
            bool present[4];
            int32_t state = 0;
            if (!ReadPresence(present, state)) return;
            bool match = state != 0 && state != 13;
            for (int s = 0; s < 4; ++s)
            {
                uint8_t gone = match && v.matchName[s][0] && !present[s] ? 1 : 0;
                if (g_slotGone[s].exchange(gone) != gone && gone) Log(L"gamepatch: %S left the match - nametag hidden", v.matchName[s]);
            }
        }

        void NametagColours()
        {
            auto twice = [](uint32_t argb) { return static_cast<uint64_t>(argb) << 32 | argb; };
            std::thread([twice]
            {
                std::wstring last;
                for (;;)
                {
                    Sleep(100);
                    // Read every pass: the in-game Settings menu can change them (nametag_colors=off: white for everyone).
                    bool colours = GetConfig().nametagColors;
                    uint64_t ally = twice(colours ? GetConfig().allyNametagColor : 0xffffffff), enemy = twice(colours ? GetConfig().enemyNametagColor : 0xffffffff);
                    g_deadColour = twice(colours ? GetConfig().deadNametagColor : 0xffffffff);
                    TeamView v{};
                    if (!ReadTeamView(v)) continue;
                    UpdateLeftPlayers(v);
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
        // The lobby ended while its Arsenal editor was open (LobbyEnded): leaving the editor then opens livegterr, the
        // lobby's own way out when its session is lost (livegterr 84 -> liveerror 53 -> liveend 52 -> mainmenu 8).
        // Going back into livelobby instead would start a match with nobody (menu 1).
        std::atomic<bool> g_lobbyEndedInEditor{ false };
        constexpr int kLobbyLostMenu = 84;

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
            // Quick Join finds nothing: no lobby this player can play (QuickJoinLabels).
            { "quick_match_hint", "No Quick Matches found", "No game sessions you can join right now" },
            // The scoreboard shows only the Global boards (PatchScoreboardMenu): their names lose " - Global".
            { "leaderboard_3", "Ratings - Global", "Ratings" },
            { "leaderboard_6", "Battle Royale Win Count - Global", "Battle Royale Wins" },
            { "leaderboard_9", "Tag Team Win Count - Global", "Tag Team Wins" },
            { "leaderboard_12", "One-on-One Win Count - Global", "One-on-One Wins" },
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
                // Non-hosts' lobby hint lines have no Invite entry: they get the Arsenal Lab button (Y, icon 5) after
                // Options, written with the text code character the line already uses (0x07).
                if (GetConfig().lobbyArsenalEditor && (strcmp(key, "lobby_client") == 0 || strcmp(key, "lobby_team_client") == 0))
                {
                    if (!CopyString(t.values[i], value, sizeof(value))) return;
                    std::string text = value;
                    size_t at = text.find("4#Options");
                    if (at != std::string::npos && at > 0 && text.find("Arsenal Lab") == std::string::npos)
                    {
                        char code = text[at - 1];
                        text.insert(at + strlen("4#Options"), std::string(1, code) + "5#Arsenal Lab");
                        std::string& stored = renamed[key];
                        stored = text;
                        t.values[i] = stored.data();
                        Log(L"gamepatch: text '%S' now has the Arsenal Lab button", key);
                    }
                    continue;
                }
                // The Ratings scoreboard's header "Rank | Player | Arsenal | Rating / Wins": the Arsenal column (each
                // player's arsenal on Xbox Live, a download) has nothing behind it now, so its heading goes. Columns are
                // "<code>NNNw<title>" pieces ended by a separator, written as the escapes \a / \f or as the characters.
                if (GetConfig().scoreboard && _stricmp(key, "msg_live_rank_list0_tag") == 0)
                {
                    if (!CopyString(t.values[i], value, sizeof(value))) return;
                    std::string text = value;
                    bool changed = false;
                    // The column starts at the text code before its position ("\a390w" or 0x07 "390w") and ends where
                    // the next column's code begins (after a "\f" / 0x0C separator, if any).
                    if (size_t at = text.find("Arsenal"); at != std::string::npos)
                    {
                        size_t from = text.find_last_of("\a\\", at);
                        size_t end = text.find_first_of("\a\\\f", at);
                        if (end != std::string::npos && text.compare(end, 2, "\\f") == 0) end += 2;
                        else if (end != std::string::npos && text[end] == '\f') end += 1;
                        if (from != std::string::npos && end != std::string::npos)
                        {
                            text.erase(from, end - from);
                            changed = true;
                        }
                    }
                    // Its wins column is every win (TotalWin), unlike the per-mode boards.
                    if (text.find("Total Wins") == std::string::npos)
                        if (size_t at = text.rfind("Wins"); at != std::string::npos)
                        {
                            text.insert(at, "Total ");
                            changed = true;
                        }
                    if (changed)
                    {
                        std::string& stored = renamed[key];
                        stored = text;
                        t.values[i] = stored.data();
                        Log(L"gamepatch: Ratings scoreboard header: no Arsenal column, Total Wins");
                    }
                    continue;
                }                for (auto& r : kRenames)
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

        std::atomic<uint64_t> g_settingsOpenedAt{ 0 }; // when menu 68 (Settings) was last opened (HookedSettingFlag)

        // The lobby's "in a menu" mark (the red hand): session cmd 62 stores this player's lobby status at 0x1405277c4 (the
        // lobby sets 1 when it shows its main LOBBY window, 0 for its Options menu and the leave prompt) and cmd 63 has it
        // sent to the others now (0x140594e90 / e94). While a player's is 0, the host can't start the match. The Arsenal
        // editor opened from the lobby (OpenLobbyArsenalEditor) sets 0 too, and back in the lobby it is 1 again.
        void SetLobbyAvailable(bool available)
        {
            *reinterpret_cast<volatile int32_t*>(At(0x1405277c4)) = available ? 1 : 0;
            *reinterpret_cast<volatile int32_t*>(At(0x140594e90)) = 1;
            *reinterpret_cast<volatile int32_t*>(At(0x140594e94)) = 1;
        }

        int HookedPushMenu(int64_t manager, int menu, char force)
        {
            int current = CurrentMenu(manager);
            if (menu == 68) g_settingsOpenedAt = GetTickCount64();
            RenameStrings(); // cheap; covers string tables loaded after start-up
            if (menu != current) Log(L"menu: open %d (from %d)", menu, current);
            if (g_editorFromLobby && current == kEditorMenu && menu != kEditorMenu && menu != kStockMenu)
            {
                g_editorFromLobby = false;
                if (g_lobbyEndedInEditor.exchange(false))
                {
                    Log(L"menu: leaving the Arsenal editor - the lobby ended meanwhile, so out the lobby's way (menu %d)", kLobbyLostMenu);
                    return g_pushMenu(manager, kLobbyLostMenu, force);
                }
                Log(L"menu: leaving the Arsenal editor opened from the lobby - back to the lobby instead of menu %d", menu);
                int result = g_popMenu();
                SetLobbyAvailable(true);
                return result;
            }
            return g_pushMenu(manager, menu, force);
        }

        int HookedPopMenu()
        {
            int64_t manager = MenuManager();
            int current = CurrentMenu(manager);
            Log(L"menu: back (from %d)", current);
            if (current == kEditorMenu && g_editorFromLobby.exchange(false))
            {
                if (g_lobbyEndedInEditor.exchange(false))
                {
                    Log(L"menu: leaving the Arsenal editor - the lobby ended meanwhile, so out the lobby's way (menu %d)", kLobbyLostMenu);
                    return g_pushMenu(manager, kLobbyLostMenu, 0);
                }
                int result = g_popMenu();
                SetLobbyAvailable(true); // back in the lobby: no red hand
                return result;
            }
            return g_popMenu();
        }

        // Replaces the 10-byte prologue `mov [rsp+8],rbx / push rdi / sub rsp,0x20` at `ghidra` with a jump to `hook`
        // (through a 14-byte absolute jump in the code cave) and returns a trampoline that runs the prologue and
        // continues in the original function.
        // The first 10 bytes of the hooked function must be `prologue` (position-independent: they run again in the
        // trampoline). Default: mov [rsp+8],rbx; push rdi; sub rsp,20h.
        constexpr uint8_t kPrologue[10] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x57, 0x48, 0x83, 0xec, 0x20 };

        // The same for any start of at least 5 bytes: `expected` is what the function starts with (all replaced by the
        // jump), `replay` what the trampoline runs in its place before continuing after it (the same bytes, unless one
        // of them is RIP-relative and has to be rewritten).
        void* InlineHookBytes(uint64_t ghidra, const std::vector<uint8_t>& expected, const std::vector<uint8_t>& replay, void* hook, const wchar_t* name)
        {
            uint8_t* site = At(ghidra);
            if (expected.size() < 5 || memcmp(site, expected.data(), expected.size()) != 0)
            {
                Log(L"gamepatch: %s hook skipped - the game code is not what this pdnet expects", name);
                return nullptr;
            }
            auto trampoline = static_cast<uint8_t*>(VirtualAlloc(nullptr, 64, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
            uint8_t* cave = CodeCave(16);
            if (!trampoline || !cave)
            {
                Log(L"gamepatch: %s hook skipped - no memory for it", name);
                return nullptr;
            }
            std::vector<uint8_t> back = replay;
            back.insert(back.end(), { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 });     // jmp [rip+0]
            Put64(back, reinterpret_cast<uint64_t>(site + expected.size()));
            memcpy(trampoline, back.data(), back.size());
            FlushInstructionCache(GetCurrentProcess(), trampoline, back.size());

            std::vector<uint8_t> toHook = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 }; // jmp [rip+0]
            Put64(toHook, reinterpret_cast<uint64_t>(hook));
            std::vector<uint8_t> jump = { 0xe9 };
            Put32(jump, Rel32(site + 5, cave));
            jump.resize(expected.size(), 0x90);
            if (!Write(cave, toHook) || !Write(site, jump))
            {
                Log(L"gamepatch: %s hook failed (VirtualProtect error %lu)", name, GetLastError());
                return nullptr;
            }
            return trampoline;
        }

        void* InlineHook(uint64_t ghidra, void* hook, const wchar_t* name, const uint8_t (&prologue)[10] = kPrologue)
        {
            std::vector<uint8_t> bytes(prologue, prologue + sizeof(prologue));
            return InlineHookBytes(ghidra, bytes, bytes, hook, name);
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
        int PatchLobbyScript(uint8_t* script, bool gamertag, bool micLabel, bool arsenal, int& labels, int& arsenalPatched)
        {
            labels = 0;
            arsenalPatched = 0;
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
                    // Arsenal Lab for everyone: the Y handler (albmain_ty) opens the invite screen (session command 318,
                    // the Arsenal editor in pdnet) only for the host: `session 32; push ret; push 0; ==; jz +3; push 0;
                    // g1#43; session 318`. The jz becomes a jump to the next word, so clients get it too.
                    if (arsenal && w[0] == 0x42380001 && w[1] == kPushResult && w[2] == 0x0000000E && w[3] == 0x00000017 && w[4] == 0x00030207 &&
                        w[5] == 0x0000000E && w[6] == 0x102B0101 && w[7] == 0x43560001)
                    {
                        w[4] = 0x00000207;
                        ++arsenalPatched;
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

        // Quick Join: the multiplayer menu (livemain, menu 36) calls its quick search "Quick Match" and describes it
        // as Quick-rule only; it now joins any lobby the player can play (MultiplayerService search, pdnet). Its two
        // texts in the loaded script's string pool are overwritten in place (the new ones are shorter).
        constexpr int kLiveMainMenu = 36;
        constexpr Relabel kLiveMainTexts[] = {
            { "Quick Match", "Quick Join" },
            { "Search for a game session with the Quick game rule.", "Join the first open game session you can play in." },
        };

        int RelabelScript(uint8_t* script, const Relabel* texts, size_t count)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                uint32_t labelsAt = code[1], strings = code[3];
                if (code[0] != 0x20) labelsAt += 4, strings += 12; // loaded: offsets relative to their own field
                if (labelsAt <= 0x20 || labelsAt > 0x40000 || strings < labelsAt || strings > 0x80000) return -1;
                const char* pool = reinterpret_cast<const char*>(script + strings);
                int done = 0;
                for (uint32_t* w = code + 0x20 / 4; w < code + labelsAt / 4; ++w)
                {
                    if ((*w & 0xffff) != 0x0011) continue; // push string
                    char* text = const_cast<char*>(pool + (*w >> 16));
                    for (size_t i = 0; i < count; ++i)
                    {
                        if (strcmp(text, texts[i].find) == 0) memcpy(text, texts[i].replace, strlen(texts[i].replace) + 1);
                        else if (strcmp(text, texts[i].replace) != 0) continue;
                        ++done;
                    }
                }
                return done;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1;
            }
        }

        // A new label longer than the old one ("OptiMatch" -> "Lobby Browser") can't be written in place. It is stored
        // in a longer text that gets a shorter replacement (`donor`: the item's description), right after that text's
        // new end, and the label's push-string instruction is pointed there. Returns 1 when the label shows `label`.
        int RelabelLonger(uint8_t* script, const char* oldLabel, const char* label, const char* donorOld, const char* donorNew)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                uint32_t labelsAt = code[1], strings = code[3];
                if (code[0] != 0x20) labelsAt += 4, strings += 12;
                if (labelsAt <= 0x20 || labelsAt > 0x40000 || strings < labelsAt || strings > 0x80000) return -1;
                char* pool = reinterpret_cast<char*>(script + strings);
                uint32_t* labelWord = nullptr;
                uint32_t donor = 0;
                bool donorFound = false;
                for (uint32_t* w = code + 0x20 / 4; w < code + labelsAt / 4; ++w)
                {
                    if ((*w & 0xffff) != 0x0011) continue;
                    const char* text = pool + (*w >> 16);
                    if (strcmp(text, oldLabel) == 0 || strcmp(text, label) == 0) labelWord = w;
                    if (strcmp(text, donorOld) == 0 || strcmp(text, donorNew) == 0) { donor = *w >> 16; donorFound = true; }
                }
                if (!labelWord || !donorFound) return 0;
                if (strcmp(pool + (*labelWord >> 16), label) == 0) return 1; // done before (a reused buffer)
                size_t room = strlen(donorOld) + 1, need = strlen(donorNew) + 1 + strlen(label) + 1;
                if (strcmp(pool + donor, donorOld) != 0 || need > room) return 0;
                memcpy(pool + donor, donorNew, strlen(donorNew) + 1);
                uint32_t at = donor + static_cast<uint32_t>(strlen(donorNew)) + 1;
                memcpy(pool + at, label, strlen(label) + 1);
                *labelWord = (*labelWord & 0xffff) | (at << 16);
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1;
            }
        }

        void QuickJoinLabels()
        {
            std::thread([]
            {
                auto entry = reinterpret_cast<uint8_t* volatile*>(At(0x1403fa3d0) + kLiveMainMenu * 0x18 + 0x10);
                uint8_t* done = nullptr;
                for (;;)
                {
                    Sleep(50);
                    // Every pass, not only when the pointer changes: the game can load the script again into the
                    // same buffer (back from the Lobby Browser), which brings the old texts back. Both are no-ops once done.
                    uint8_t* script = *entry;
                    bool fresh = script != done;
                    done = script;
                    if (!script) continue;
                    int quick = RelabelScript(script, kLiveMainTexts, std::size(kLiveMainTexts));
                    int browser = RelabelLonger(script, "OptiMatch", "Lobby Browser",
                        "Select game attributes, and then search for a game session.", "Choose search options, then pick a session.");
                    if (fresh) Log(L"gamepatch: multiplayer menu: Quick Match renamed Quick Join (%d of 2 texts), OptiMatch renamed Lobby Browser (%d of 1)", quick, browser);
                }
            }).detach();
        }

        // Rated / Unrated lobbies (rated_lobbies=on). The create-session menu's Rating option (No Restrictions /
        // With Restrictions: session number ratinglimit 0 / 1, originally "only players within 200 of the host")
        // becomes Unrated / Rated; the lobby search's Rating option then finds one kind or the other, and the ±200
        // limit is gone (xsapi/stubs/Multiplayer.MultiplayerService.cpp). A match changes ratings only when Rated:
        // FUN_1401835c0 (rating += change, at match start and end) is hooked and gets a change of 0 otherwise. The
        // game rated only the three "Official Battle" rules (rule < 3, checks at 0x1401878c6 and 0x1401882c1); those
        // checks are removed, so any rule can be Rated.
        constexpr Relabel kRatedTexts[] = {
            { "No Restrictions", "Unrated" },
            { "With Restrictions", "Rated" },
            { "With Restrictions (%d\xEF\xBD\x9E%d)(You %d)", "Rated" },
            { "Change the settings for the game session.\nSelect rating restrictions.",
              "Change the settings for the game session.\nRated: matches change ratings." },
            { "Your Rating: %d\n\nIf a rating limit is selected, only those 200 above or\nbelow your rating will be allowed to join.",
              "Your Rating: %d\n\nRated: each match changes your rating.\nUnrated: your rating stays as it is." },
            { "Create conditions for game session search.\nSelect whether or not to look for games with rating restrictions.",
              "Create conditions for game session search.\nLook for rated or unrated games." },
        };
        constexpr char kOfficialBattle[] = "[Official Battle] Applies toward Scoreboard Total\n";

        // Removes the "[Official Battle]" line from the rule descriptions (any rule can be Rated now).
        int TrimOfficialBattle(uint8_t* script)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                uint32_t labelsAt = code[1], strings = code[3];
                if (code[0] != 0x20) labelsAt += 4, strings += 12;
                if (labelsAt <= 0x20 || labelsAt > 0x40000 || strings < labelsAt || strings > 0x80000) return -1;
                char* pool = reinterpret_cast<char*>(script + strings);
                int done = 0;
                for (uint32_t* w = code + 0x20 / 4; w < code + labelsAt / 4; ++w)
                {
                    if ((*w & 0xffff) != 0x0011) continue;
                    char* text = pool + (*w >> 16);
                    if (strncmp(text, kOfficialBattle, sizeof(kOfficialBattle) - 1) != 0) continue;
                    memmove(text, text + sizeof(kOfficialBattle) - 1, strlen(text + sizeof(kOfficialBattle) - 1) + 1);
                    ++done;
                }
                return done;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1;
            }
        }

        // The scoreboard menu (liveranking) lists 12 boards, Friends / Favorites / Global of Ratings, Battle Royale,
        // Tag Team and One-on-One ($LEADERBOARD_1..12). Only the Global ones are left (scoreboard=on): menu command 40
        // (add item, 0x140207e20) skips the other eight as the list is built (HookedAddItem; the script runs before a
        // watcher could change it), and the first four items' handlers store board 3/6/9/12 instead of 1-4, so the rest
        // of the menu works on the Global board as before.
        constexpr int kRankingMenu = 41;

        int PatchScoreboardMenu(uint8_t* script)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                constexpr uint32_t kHandlers[4] = { 0x30F8 / 4, 0x3124 / 4, 0x3150 / 4, 0x317C / 4 };
                if (code[kHandlers[0]] == (0x000Eu | 3u << 16) && code[kHandlers[3]] == (0x000Eu | 12u << 16)) return 1; // done before
                for (int b = 0; b < 4; ++b)
                    if (code[kHandlers[b]] != (0x000Eu | static_cast<uint32_t>(b + 1) << 16) || (code[kHandlers[b] + 1] & 0xffff) != 0x0103) return 0; // push N ; call-label (store the board)
                for (int b = 0; b < 4; ++b) code[kHandlers[b]] = 0x000Eu | static_cast<uint32_t>(b * 3 + 3) << 16;
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }

        using MenuCommandFn = int64_t (*)(int64_t* args);
        MenuCommandFn g_addItem = nullptr;

        // The multiplayer menu's items and their descriptions, renamed as the list is built (a watcher that patches
        // the script after it loads is too late for the first draw): Quick Join and Lobby Browser.
        constexpr Relabel kMenuItems[] = {
            { "Quick Match", "Quick Join" },
            { "OptiMatch", "Lobby Browser" },
            { "Search for a game session with the Quick game rule.", "Join the first open game session you can play in." },
            { "Select game attributes, and then search for a game session.", "Choose search options, then pick a session." },
            { "Xbox Live Options", "Settings" },
            { "Display the Options menu.", "Your name, nametags, start-up volume and Discord posts." },
        };

        bool SettingsItems(const char* added); // the in-game Settings rows (below)
        bool SoundItems(const char* added, int64_t* args, int64_t& result); // the Options > Sound rows (below)

        int64_t HookedAddItem(int64_t* args)
        {
            if (args)
            {
                char text[80];
                if (CopyString(reinterpret_cast<const char*>(args[0]), text, sizeof(text)))
                {
                    // Only the scoreboard menu adds $LEADERBOARD_ items.
                    if (GetConfig().scoreboard && strncmp(text, "$LEADERBOARD_", 13) == 0 && atoi(text + 13) % 3 != 0) return 0; // Friends / Favorites
                    for (auto& item : kMenuItems)
                        if (strcmp(text, item.find) == 0)
                        {
                            int64_t renamed[8];
                            memcpy(renamed, args, sizeof(renamed));
                            renamed[0] = reinterpret_cast<int64_t>(item.replace); // static text: the menu may keep the pointer
                            return g_addItem(renamed);
                        }
                    if (SettingsItems(text)) return 0; // replaced by the in-game Settings rows
                    if (int64_t result = 0; SoundItems(text, args, result)) return result; // followed by the Options > Sound rows
                    return g_addItem(args);
                }
            }
            return g_addItem(args);
        }
        void SettingsScriptPatch(int menu, uint8_t* script, bool fresh); // the in-game Settings menu (below)
        void SettingsTick();

        // Every menu script as it loads (menu table, 87 entries): the Rated texts, and the scoreboard list.
        void MenuTextPatches(bool rated, bool scoreboard)
        {
            std::thread([rated, scoreboard]
            {
                constexpr int kMenus = 87;
                uint8_t* done[kMenus] = {};
                for (;;)
                {
                    Sleep(50);
                    SettingsTick();
                    for (int m = 2; m < kMenus; ++m)
                    {
                        uint8_t* script = *reinterpret_cast<uint8_t* volatile*>(At(0x1403fa3d0) + m * 0x18 + 0x10);
                        bool fresh = script != done[m];
                        done[m] = script;
                        if (!script) continue;
                        // Every pass (all no-ops once done): a script loaded again into the same buffer has its old texts.
                        if (rated)
                        {
                            int texts = RelabelScript(script, kRatedTexts, std::size(kRatedTexts)), official = TrimOfficialBattle(script);
                            if (fresh && (texts > 0 || official > 0)) Log(L"gamepatch: menu %d: %d Rated text(s), %d Official Battle line(s) removed", m, texts, official);
                        }
                        SettingsScriptPatch(m, script, fresh);
                        if (scoreboard && m == kRankingMenu)
                        {
                            int patched = PatchScoreboardMenu(script);
                            if (fresh) Log(L"gamepatch: scoreboard menu %s", patched ? L"items open the Global boards" : L"left as it is (script not as expected)");
                        }
                    }
                }
            }).detach();
        }

        using ApplyRatingFn = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
        ApplyRatingFn g_applyRating = nullptr;

        // A finished online match (the end call, from FUN_140188290) puts the player on the scoreboard right away,
        // Rated or Unrated: rating, name and 0 wins on that mode's board (scores::MatchPlayed). Wins themselves come
        // from the game's win events (Events.EventsService.cpp): the rating change is no guide, a Battle Royale win
        // can lose rating points.
        // A match's rating: at the start the game takes a deposit (FUN_140187710 calls with a negative change), at
        // the end it gives back the deposit plus the result (FUN_140188290). The result is evened out so a win always
        // nets at least +1 and a loss (no win event this match, see NoteMatchWin) at least -1: the game's own maths
        // can make a Battle Royale win cost points. Only the end change is adjusted.
        int64_t g_matchStartChange = 0;
        bool g_matchStarted = false;
        std::atomic<bool> g_matchWon{ false };

        int64_t HookedApplyRating(int64_t a, int64_t change, int64_t c, int64_t d, int64_t e, int64_t f)
        {
            auto from = reinterpret_cast<uint8_t*>(_ReturnAddress());
            bool atStart = from >= At(0x140187710) && from < At(0x140187a20), atEnd = from >= At(0x140188290) && from < At(0x140188460);
            if (atStart)
            {
                g_matchStartChange = change;
                g_matchStarted = true;
                g_matchWon = false;
            }
            else if (atEnd && g_matchStarted)
            {
                g_matchStarted = false;
                int64_t net = g_matchStartChange + change, wanted = net;
                if (g_matchWon && net < 1) wanted = 1;
                if (!g_matchWon && net > -1) wanted = -1;
                if (wanted != net)
                {
                    Log(L"gamepatch: %s would net %+lld, made %+lld", g_matchWon ? L"win" : L"loss", net, wanted);
                    change += wanted - net;
                }
            }
            static int last = -1;
            int rated = scores::RatedMatch();
            if (rated < 0) rated = last; // the session can be gone by the end of the match
            else last = rated;
            if (rated != 1)
            {
                Log(L"gamepatch: rating change %lld not applied (%s match)", change, rated == 0 ? L"Unrated" : L"unknown -> Unrated");
                change = 0;
            }
            else Log(L"gamepatch: rating change %lld (Rated match)", change);
            int64_t result = g_applyRating(a, change, c, d, e, f);
            if (atEnd && gamepatches::MultiplayerProfile())
            {
                static const char* kModes[] = { "BattleRoyaleWin", "TagTeamWin", "OneOnOneWin" };
                int type = *reinterpret_cast<int32_t*>(At(0x1405277b4));
                scores::MatchPlayed(type >= 0 && type < 3 ? kModes[type] : "", gamepatches::SavedRating());
            }
            return result;
        }
        void RatedLobbies()
        {
            uint8_t* start = At(0x1401878c6); // cmp edi,2 ; jg  (match start: rule > 2 -> no rating)
            uint8_t* end = At(0x1401882c1);   // cmp dword [rule],2 ; ja  (match end)
            if (memcmp(start, "\x83\xff\x02\x7f", 4) != 0 || memcmp(end, "\x83\x3d\x34\xf5\x39\x00\x02\x0f\x87", 9) != 0)
            {
                Log(L"gamepatch: Rated lobbies skipped - the game code is not what this pdnet expects");
                return;
            }
            constexpr uint8_t kRatingPrologue[10] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10 }; // mov [rsp+8],rbx ; mov [rsp+10h],rbp
            auto original = reinterpret_cast<ApplyRatingFn>(InlineHook(0x1401835c0, reinterpret_cast<void*>(&HookedApplyRating), L"rating change", kRatingPrologue));
            if (!original) return;
            g_applyRating = original;
            if (!Write(start + 3, { 0x90, 0x90 }) || !Write(end + 7, { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 }))
            {
                Log(L"gamepatch: Rated lobbies: rule checks not removed (VirtualProtect error %lu)", GetLastError());
                return;
            }
            Log(L"gamepatch: Rated lobbies ready (the lobby's Rating option: Unrated / Rated, any rule)");
        }

        // The shop's Online Store item (multiplayer only; Xbox store, dead without Xbox Live) becomes Max Credits &
        // Skills. Its handler calls the store routine at 0x1F04 (header, then g9 #3 with 1 starts fetching offers).
        // That routine is rewritten to push kMaxOutCode and call g9 #3, whose table entry is wrapped (HookedStoreStart)
        // to top the active profile up instead (MaxOut). The store's own texts (pool 1643-1736,
        // used only by the store's states) hold the item's new label and hint.
        constexpr int kShopMenu = 6;
        constexpr int64_t kMaxOutCode = 2001;
        using NativeFn = int64_t (*)(int64_t* args);
        NativeFn g_storeStart = nullptr;

        // The game's own reward routines (daily rewards, FUN_1400bb6c0). mode 1: story profile, 2: multiplayer profile.
        // Skills by catalog number from-to, each + count, capped at 99 (FUN_14015aae0); credits capped at 999,999,999;
        // an arsenal case with a given school restriction (FUN_14015c0e0; the case's +0x50, as the arsenal shop sells).
        using GiveSkillsFn = void (*)(uint32_t mode, int from, int to, int count);
        using GiveCreditsFn = void (*)(uint32_t mode, int amount);
        using GiveCaseFn = void (*)(uint32_t mode, int schools);
        constexpr int kSkillTarget = 80, kCreditTarget = 5000000, kCaseSchools = 2;

        // Tops the active profile up, so pressing it again gives nothing more: every skill to 80 (catalog numbers 1-999,
        // the internal id at 0x1404bcca0 + n*4, its count at *(save + 0x3a38) + id), credits to 5,000,000, and one
        // 2-School Arsenal Case unless the profile already has one (cases: *(save + 0x10a*0x38), 100 bytes each,
        // count at save + 0x3a48, at most 16).
        void MaxOut()
        {
            uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
            if (!save) return;
            uint32_t mode = *reinterpret_cast<uint8_t**>(save + 0x33a0) == save + 0x3fb8 ? 2 : 1;
            auto give = reinterpret_cast<GiveSkillsFn>(At(0x14015bdd0));
            auto ids = reinterpret_cast<const int32_t*>(At(0x1404bcca0));
            int skills = 0;
            for (int n = 1; n < 1000; ++n)
            {
                int id = ids[n];
                if (id < 1 || id > 999) continue;
                int have = (*reinterpret_cast<int8_t**>(save + 0x3a38))[id]; // re-read: the routine re-points it
                if (have >= kSkillTarget) continue;
                give(mode, n, n, kSkillTarget - have);
                ++skills;
            }
            auto& credits = *reinterpret_cast<int32_t*>(At(0x14059a9d8));
            int before = credits;
            if (credits < kCreditTarget) reinterpret_cast<GiveCreditsFn>(At(0x14015bf90))(mode, kCreditTarget - credits);
            int cases = *reinterpret_cast<int32_t*>(save + 0x3a48);
            uint8_t* list = *reinterpret_cast<uint8_t**>(save + 0x10a * 0x38);
            bool hasCase = false;
            for (int i = 0; list && i < cases && i < 16; ++i) hasCase |= *reinterpret_cast<int16_t*>(list + i * 100 + 0x50) == kCaseSchools;
            bool gaveCase = !hasCase && cases < 16;
            if (gaveCase) reinterpret_cast<GiveCaseFn>(At(0x14015c0e0))(mode, kCaseSchools);
            Log(L"gamepatch: shop Max Credits & Skills (%s profile): %d skill(s) topped up to %d, credits %d -> %d, 2-School Arsenal Case %s",
                mode == 2 ? L"multiplayer" : L"story", skills, kSkillTarget, before, credits, gaveCase ? L"given" : hasCase ? L"already owned" : L"not given (16 cases)");
        }
        int64_t HookedStoreStart(int64_t* args)
        {
            if (!args || args[0] != kMaxOutCode) return g_storeStart(args);
            MaxOut();
            return 0;
        }

        // Returns 1 when patched (now or before), 0 when the script is not the expected one.
        int PatchShopScript(uint8_t* script)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                uint32_t labelsAt = code[1], strings = code[3];
                if (code[0] != 0x20) labelsAt += 4, strings += 12;
                if (labelsAt <= 0x2400 || labelsAt > 0x40000 || strings < labelsAt || strings > 0x80000) return 0;
                char* pool = reinterpret_cast<char*>(script + strings);
                const uint32_t routine[3] = { 0x0000000Eu | static_cast<uint32_t>(kMaxOutCode) << 16, 0x90030101, 0x00000005 };
                uint32_t* at = code + 0x1F04 / 4;
                if (memcmp(at, routine, sizeof(routine)) == 0) return 1;
                if (at[0] != 0x0000000E || at[1] != 0x069E0011 || at[2] != 0x0001000E || at[3] != 0x20980301 || at[5] != 0x90030101 ||
                    code[0x23F4 / 4] != 0xFEC30003 || code[0x1BD0 / 4] != 0x05290011 || code[0x1C58 / 4] != 0x064B0011 ||
                    strcmp(pool + 1643, "$STORE_TITLE") != 0 || strcmp(pool + 1725, "$STORE_WAIT") != 0)
                    return 0;
                const char label[] = "Max Credits & Skills";
                const char hint[] = "Up to 5,000,000 credits, 80 of each skill, one 2-School Arsenal Case.";
                static_assert(sizeof(label) + sizeof(hint) <= 1737 - 1643, "store texts too short");
                memcpy(pool + 1643, label, sizeof(label));
                memcpy(pool + 1643 + sizeof(label), hint, sizeof(hint));
                code[0x1BD0 / 4] = 0x0011u | 1643u << 16;
                code[0x1C58 / 4] = 0x0011u | static_cast<uint32_t>(1643 + sizeof(label)) << 16;
                memcpy(at, routine, sizeof(routine));
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }

        void ShopMaxOut()
        {
            auto entry = reinterpret_cast<NativeFn*>(At(0x1403dd3e0) + 3 * 8);
            DWORD old = 0;
            if (*entry != reinterpret_cast<NativeFn>(At(0x1400bc710)) || memcmp(At(0x14015bdd0), "\x4c\x8b\xdc\x53", 4) != 0 ||
                memcmp(At(0x14015bf90), "\x48\x89\x5c\x24\x08", 5) != 0 || memcmp(At(0x14015c0e0), "\x48\x89\x5c\x24\x08\x48\x89\x6c\x24\x18", 10) != 0)
            {
                Log(L"gamepatch: shop Max Credits & Skills skipped - the game code is not what this pdnet expects");
                return;
            }
            if (!VirtualProtect(entry, 8, PAGE_READWRITE, &old)) return;
            g_storeStart = *entry;
            *entry = &HookedStoreStart;
            VirtualProtect(entry, 8, old, &old);
            std::thread([]
            {
                auto script = reinterpret_cast<uint8_t* volatile*>(At(0x1403fa3d0) + kShopMenu * 0x18 + 0x10);
                uint8_t* done = nullptr;
                for (;;)
                {
                    Sleep(50);
                    uint8_t* now = *script;
                    bool fresh = now != done;
                    done = now;
                    if (!now) continue;
                    int patched = PatchShopScript(now); // every pass: a reload into the same buffer brings the old script back
                    if (fresh) Log(L"gamepatch: shop: Online Store %s", patched ? L"is now Max Credits & Skills" : L"left as it is (script not as expected)");
                }
            }).detach();
        }

        void LobbyScriptPatches(bool gamertag, bool micLabel, bool arsenal)
        {
            // Each load is patched once; the entry goes back to null when the lobby closes (QuitMenu frees the
            // script), which also covers the next load reusing the same buffer.
            std::thread([gamertag, micLabel, arsenal]
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
                    int labels = 0, arsenalPatched = 0;
                    int n = PatchLobbyScript(script, gamertag, micLabel, arsenal, labels, arsenalPatched);
                    if (gamertag)
                    {
                        if (n == 3) Log(L"gamepatch: lobby Gamertag option enabled (3 script checks)");
                        else Log(L"gamepatch: lobby Gamertag option - %d of 3 script checks matched (script at %p)", n, script);
                    }
                    if (micLabel) Log(L"gamepatch: lobby Voice Chat option relabelled Mic (%d of 2 texts)", labels);
                    if (arsenal) Log(L"gamepatch: lobby Arsenal Lab button for every player (%d of 1 check)", arsenalPatched);
                }
            }).detach();
            Log(L"gamepatch: lobby script watcher started (Gamertag option %s, Mic label %s, Arsenal Lab for all %s)", gamertag ? L"on" : L"off", micLabel ? L"on" : L"off", arsenal ? L"on" : L"off");
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

        // SESSION INFO (lobby browser, menu 43) draws "Game Rule: %s" with menu command 23 for the session under the
        // cursor (menu command 55 with 1, 9 gives its row; rows are the last search's results, in order): the rule
        // gets " (Rated)" or " (Unrated)" after it.
        constexpr int kSessionListMenu = 43;

        // In-game Settings: the multiplayer menu's Xbox Live Options (liveoption, menu 68; its Friends List is dead
        // without Xbox Live) becomes Settings. The Friends List shows one row per setting: session command 85 (friend
        // count) and 86 (row text, a pointer the list keeps, so a row's buffer can change while it shows) answer for
        // the settings; 87-90, 159, 160 (friend status icons) answer 0; 154 (A on a row) changes that setting, and the
        // script's next step (open the friend's command menu, call-label at 0x35BC) is patched out. The info panel's
        // "Offline" / "Voice: Off" lines show the selected setting's description (menu command 23; session command 104
        // tells which row the panel is for). Changes go to pdnet.ini (SaveSetting) and apply at once where they can.
        constexpr int kSettingsMenu = 68;
        bool InSettings() { return CurrentMenu(MenuManager()) == kSettingsMenu; }
        struct Colour { const char* name; uint32_t rgb; };
        constexpr Colour kColours[] = { { "Yellow", 0xFFE600 }, { "Red", 0xFF5A5A }, { "Blue", 0x4FB4FF }, { "Green", 0x5AFF6E },
                                        { "Purple", 0xB46EFF }, { "Orange", 0xFFA03C }, { "Cyan", 0x3CF0FF }, { "Pink", 0xFF78C8 },
                                        { "White", 0xFFFFFF } };
        enum SettingRow { kName, kVoiceMode, kBodyTags, kTagColours, kDeadColour, kAllyColour, kEnemyColour, kSplashVolume,
                          kDiscordLobby, kDiscordPlayers, kSettingRows };
        char g_rowText[kSettingRows][80];
        char g_optionText[kSettingRows][96]; // the rows as the Options screen shows them (RefreshSettingRows)
        constexpr int kFirstOptionRow = 3;   // after Cross Play (1) and Voice Chat (2)
        const char* kOptionLabels[kSettingRows] = { "Name", "Voice Mode", "Body Tags", "Tag Colours", "Dead Colour", "Ally Colour", "Foe Colour",
                                                    "Logo Volume", "Discord Lobby", "Discord Join" };
        std::atomic<bool> g_typingName{ false };
        std::atomic<bool> g_settingsDirty{ false }; // changed in the Settings screen since it opened or was last saved
        std::atomic<bool> g_confirmingSave{ false }; // "Save all settings?" is showing (HookedButtons)
        Config g_settingsSaved;                     // the settings as they were then (restored when leaving unsaved)
        std::wstring g_typedName;
        std::mutex g_typedLock;


        std::string ColourName(uint32_t argb)
        {
            for (auto& c : kColours)
                if ((argb & 0xFFFFFF) == c.rgb) return c.name;
            char hex[16];
            sprintf_s(hex, "#%06X", argb & 0xFFFFFF);
            return hex;
        }

        uint32_t NextColour(uint32_t argb, int direction)
        {
            size_t n = std::size(kColours);
            for (size_t i = 0; i < n; ++i)
                if ((argb & 0xFFFFFF) == kColours[i].rgb) return 0xFF000000u | kColours[(i + n + direction) % n].rgb;
            return 0xFF000000u | kColours[0].rgb;
        }

        void RefreshSettingRows()
        {
            const Config& c = GetConfig();

            auto onOff = [](bool on) { return on ? "On" : "Off"; };
            std::string name;
            {
                std::lock_guard guard(g_typedLock);
                name = winrt::to_string(g_typingName ? g_typedName + L"_" : c.displayName);
            }
            sprintf_s(g_rowText[kName], "Name: %s", name.c_str());
            sprintf_s(g_rowText[kVoiceMode], "Voice mode: %s", c.voiceClassic ? "Classic" : "Modern");
            sprintf_s(g_rowText[kBodyTags], "Nametags on bodies: %s", onOff(c.nametagsOnBodies));
            sprintf_s(g_rowText[kTagColours], "Nametag colours: %s", onOff(c.nametagColors));
            sprintf_s(g_rowText[kDeadColour], "Dead colour: %s", ColourName(c.deadNametagColor).c_str());
            sprintf_s(g_rowText[kAllyColour], "Teammate colour: %s", ColourName(c.allyNametagColor).c_str());
            sprintf_s(g_rowText[kEnemyColour], "Opponent colour: %s", ColourName(c.enemyNametagColor).c_str());
            sprintf_s(g_rowText[kSplashVolume], "Logo volume: %d%%", c.splashVolume);
            sprintf_s(g_rowText[kDiscordLobby], "Discord lobby posts: %s", onOff(c.discordLobbyMessages));
            sprintf_s(g_rowText[kDiscordPlayers], "Discord player posts: %s", onOff(c.discordPlayerMessages));
            for (int r = 0; r < kSettingRows; ++r)
            {
                const char* value = strchr(g_rowText[r], ':');
                sprintf_s(g_optionText[r], "\a160w%s\f\a1#\a230c%s\f\a0#", kOptionLabels[r], value ? value + 2 : "");
            }
        }

        const char* SettingHelp(int row)
        {
            switch (row)
            {
            case kName: return "Left / right, then type a new name: Enter keeps it, Esc drops it.";
            case kVoiceMode: return "Modern: the lobby's Mic option mutes only your mic. Classic: Voice Chat Off = no talking, no hearing.";
            case kBodyTags: return "Defeated players keep their nametag (Tag Team; the host's applies).";
            case kTagColours: return "Off: every nametag in the game's white.";
            case kDeadColour: return "Nametag colour of a defeated player.";
            case kAllyColour: return "Nametag colour of your teammates (Tag Team).";
            case kEnemyColour: return "Nametag colour of your opponents.";
            case kSplashVolume: return "Volume from the start-up logos until the main menu (next start).";
            case kDiscordLobby: return "Post 'lobby opened / closed' when you host.";
            case kDiscordPlayers: return "Post '<player> joined / left' when you host.";
            }
            return "";
        }

        // Name typing: left / right on the Name row starts it; typed characters are added, Backspace removes, Enter
        // keeps the name, Esc drops it (TypingThread). Leaving the Settings screen drops it too.
        void FinishTyping(bool keep)
        {
            std::wstring name;
            {
                std::lock_guard guard(g_typedLock);
                name = g_typedName;
            }
            g_typingName = false;
            if (keep && !name.empty())
            {
                EditableConfig().displayName = name.substr(0, 32);
                g_settingsDirty = true;
                Log(L"gamepatch: settings: name typed: '%s' (A saves)", EditableConfig().displayName.c_str());
            }
            RefreshSettingRows();
        }

        // Keys come from the game's own window (a UWP CoreWindow: GetAsyncKeyState sees nothing there). Its character and
        // key-down events are subscribed once, on the window's thread; while a name is being typed they go to the name
        // and are marked handled.
        void TypingThread()
        {
            static std::once_flag once;
            std::call_once(once, []
            {
                try
                {
                    using namespace winrt::Windows::UI::Core;
                    auto window = winrt::Windows::ApplicationModel::Core::CoreApplication::MainView().CoreWindow();
                    window.Dispatcher().RunAsync(CoreDispatcherPriority::Normal, [window]
                    {
                        window.CharacterReceived([](CoreWindow const&, CharacterReceivedEventArgs const& e)
                        {
                            if (!g_typingName) return;
                            if (!InSettings()) { FinishTyping(false); return; } // left the Settings screen
                            e.Handled(true);
                            uint32_t ch = e.KeyCode();
                            if (ch == 13) { FinishTyping(true); return; }
                            if (ch == 27) { FinishTyping(false); return; }
                            {
                                std::lock_guard guard(g_typedLock);
                                if (ch == 8) { if (!g_typedName.empty()) g_typedName.pop_back(); }
                                else if (ch >= 32 && ch < 0xFFFF && g_typedName.size() < 32) g_typedName += static_cast<wchar_t>(ch);
                            }
                            RefreshSettingRows();
                        });
                        window.KeyDown([](CoreWindow const&, KeyEventArgs const& e) { if (g_typingName && InSettings()) e.Handled(true); });
                        Log(L"gamepatch: settings: keyboard input for names ready");
                    });
                }
                catch (...)
                {
                    Log(L"gamepatch: settings: no keyboard input for names (0x%08X)", winrt::to_hresult());
                }
            });
        }
        void ChangeSetting(int row, int direction)
        {
            Config& c = EditableConfig();
            switch (row)
            {
            case kName:
                if (g_typingName) { FinishTyping(true); return; }
                {
                    std::lock_guard guard(g_typedLock);
                    g_typedName.clear();
                }
                g_typingName = true;
                TypingThread();
                Log(L"gamepatch: settings: typing a new name");
                break;
            case kVoiceMode: c.voiceClassic = !c.voiceClassic; break;
            case kBodyTags: c.nametagsOnBodies = !c.nametagsOnBodies; UseOwnSettings(); break;
            case kTagColours: c.nametagColors = !c.nametagColors; break;
            case kDeadColour: c.deadNametagColor = NextColour(c.deadNametagColor, direction); break;
            case kAllyColour: c.allyNametagColor = NextColour(c.allyNametagColor, direction); break;
            case kEnemyColour: c.enemyNametagColor = NextColour(c.enemyNametagColor, direction); break;
            case kSplashVolume: c.splashVolume = (std::clamp)((c.splashVolume + 5) / 10 * 10 + direction * 10, 0, 100); break;
            case kDiscordLobby: c.discordLobbyMessages = !c.discordLobbyMessages; break;
            case kDiscordPlayers: c.discordPlayerMessages = !c.discordPlayerMessages; break;
            }
            if (row != kName) g_settingsDirty = true;
            RefreshSettingRows();
        }



        // The Options screen's rows: Cross Play (1), Voice Chat (2), then pdnet's rows (3 on). Each row is one list
        // item "\a160w<label>\a1#\a230c<value>\a0#"; its draw callback (DRAW_OTHER) formats the item text, so pdnet's
        // rows (g_optionText) show their value baked in. Their hints follow the Voice Chat hint (HookedAddItem).

        // Left / right on a row the script doesn't know: the case's "pop 1" at 0x05814 became a call to session
        // command 154 (argument: the row), wrapped here. Left is button bit 0x4 (as the script tests it).
        SessionCommandFn g_optionChanged = nullptr;
        using InputFn = int64_t (*)(int64_t* args);

        int64_t HookedOptionChanged(int32_t* args)
        {
            if (!InSettings() || !args) return g_optionChanged ? g_optionChanged(args) : 0;
            int64_t input[2] = { 0, 0 };
            int64_t buttons = reinterpret_cast<InputFn>(At(0x1402168a0))(input); // g1 #37 (0): the buttons, as the script reads them
            int row = args[0] - kFirstOptionRow;
            if (row >= 0 && row < kSettingRows)
            {
                int64_t sound[2] = { 37, 0 };
                reinterpret_cast<InputFn>(At(0x140261750))(sound); // g1 #43 (37): the change sound the game's own rows play
                ChangeSetting(row, (buttons & 0x00040004) ? -1 : 1);
            }
            return 0;
        }

        constexpr Relabel kSettingsTexts[] = {
            { "XBOX LIVE OPTIONS", "SETTINGS" },
        };

        int PatchSettingsScript(uint8_t* script)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                if (code[0x05810 / 4] != 0x00010007) return 0;              // jump over the "pop 1"
                bool done = code[0x05814 / 4] == 0x42B20101;
                if (!done && (code[0x05814 / 4] != 0x0001000D || code[0x0564C / 4] != 0x0001000E || code[0x05670 / 4] != 0x0002000E ||
                              code[0x05674 / 4] != 0xFFFF000A || code[0x05418 / 4] != 0x00000010 || code[0x054A4 / 4] != 0x00000010 ||
                              code[0x05660 / 4] != 0x00000010 || code[0x058A4 / 4] != 0x00000010)) return 0;
                // Cross Play (its row, hint and detail box) is left out: its "is it shown" checks (g10 #3 with 3) read 0.
                for (uint32_t at : { 0x05418u, 0x054A4u, 0x05660u, 0x058A4u }) code[at / 4] = 0x0000000E; // push 0 (was push ret)
                code[0x05814 / 4] = 0x42B20101;                              // call session command 154 argc 1 (consumes the row)
                // Rows: cursor row + 1 is the script's row (1 Cross Play, 2 Voice Chat). Both are gone, so every cursor row
                // moves on two more: "if row == 1: row = 2" becomes "if row == row: row += 2" (cursor 0 -> 3, pdnet's first).
                code[0x0564C / 4] = 0xFFFF0009;                              // push var[-1] (was push 1)
                code[0x05670 / 4] = 0x0002000E;                              // push 2
                code[0x05674 / 4] = 0xFFFF010A;                              // var[-1] += (was store)
                return 1;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }
        // The Options screen's Voice Chat row (and its hint) is replaced by pdnet's rows (and hints); Cross Play is
        // left out by PatchSettingsScript. Returns true when it added them instead of `added`.
        // g10 #3 (0x1400bde50, the game's settings flags): flag 3 means "Cross Play can be chosen" and is always 1 on
        // PC. While the Settings screen opens (its setup runs inside the menu-open call, before it becomes the current
        // menu: the first 0.5 s after HookedPushMenu saw it) and while it is on screen it reads 0, so Cross Play (row,
        // hint, detail box) is left out. Elsewhere it stays 1: the multiplayer menu shows its Settings item only when it
        // is, and errors out when it reads 0 there.
        NativeFn g_settingFlag = nullptr;
        int64_t HookedSettingFlag(int64_t* args)
        {
            bool settings = InSettings() || GetTickCount64() - g_settingsOpenedAt < 500;
            return args && args[0] == 3 && settings ? 0 : g_settingFlag(args);
        }

        // The Settings list holds more rows than the Options screen was laid out for, so its window (menu command 37,
        // position 56, 89; second argument the top) starts 24 higher, clear of the hint line at the bottom.
        MenuCommandFn g_placeWindow = nullptr;
        int64_t HookedPlaceWindow(int64_t* args)
        {
            if (args && args[0] == 56 && args[1] == 89 && (InSettings() || GetTickCount64() - g_settingsOpenedAt < 500))
            {
                int64_t moved[8];
                memcpy(moved, args, sizeof(moved));
                moved[1] = 65;
                return g_placeWindow(moved);
            }
            return g_placeWindow(args);
        }

        // Save / leave. Changes show at once (nametag colours too) but are written only when A is pressed (the button bar
        // reads "A Save"); leaving the screen unsaved puts the settings back as they were.
        void SaveAllSettings()
        {
            if (g_typingName) FinishTyping(true); // a name being typed counts as entered
            const Config& c = EditableConfig();
            auto onOff = [](bool on) { return std::string(on ? "on" : "off"); };
            auto hex = [](uint32_t argb) { char s[8]; sprintf_s(s, "%06X", argb & 0xFFFFFF); return std::string(s); };
            SaveSettings({ { "display_name", winrt::to_string(c.displayName) }, { "voice_mode", c.voiceClassic ? "classic" : "mic" },
                           { "nametags_on_bodies", onOff(c.nametagsOnBodies) }, { "nametag_colors", onOff(c.nametagColors) },
                           { "dead_nametag_color", hex(c.deadNametagColor) }, { "ally_nametag_color", hex(c.allyNametagColor) },
                           { "enemy_nametag_color", hex(c.enemyNametagColor) }, { "splash_volume", std::to_string(c.splashVolume) },
                           { "discord_lobby_messages", onOff(c.discordLobbyMessages) },
                           { "discord_player_messages", onOff(c.discordPlayerMessages) } });
            g_settingsSaved = c;
            g_settingsDirty = false;
        }

        void RevertSettings()
        {
            Config& c = EditableConfig();
            const Config& s = g_settingsSaved;
            c.displayName = s.displayName; c.voiceClassic = s.voiceClassic; c.nametagsOnBodies = s.nametagsOnBodies; c.nametagColors = s.nametagColors;
            c.deadNametagColor = s.deadNametagColor; c.allyNametagColor = s.allyNametagColor; c.enemyNametagColor = s.enemyNametagColor;
            c.splashVolume = s.splashVolume;
            c.discordLobbyMessages = s.discordLobbyMessages; c.discordPlayerMessages = s.discordPlayerMessages;
            g_settingsDirty = false;
            UseOwnSettings();
            Log(L"gamepatch: settings: left without saving - changes dropped");
        }

        // Every 50 ms (MenuTextPatches): leaving the Settings screen with unsaved changes drops them.
        void SettingsTick()
        {
            if (!InSettings()) g_confirmingSave = false;
            if (!InSettings() && GetTickCount64() - g_settingsOpenedAt > 1000 && (g_settingsDirty || g_typingName))
            {
                g_typingName = false;
                RevertSettings();
            }
        }

        // A asks to save: "Save all settings?" in a pop-up window (OpenSavePopup) and the button bar; while it shows,
        // A saves everything (a name being typed counts as entered), B cancels, and no other button reaches the screen.
        // The buttons come from g1 #37 (0x1402168a0, wrapped: the script reads them there; A is 0x1010, B 0x2020).
        // The kit's name on the main menu, under the game's own "Skill Update 1.40 applied" in the bottom-right corner:
        // the first line of kit-version.txt next to pdnet ("PhantomDust-pdnet Experimental 29" -> "pdnet Experimental 29"),
        // drawn by HookedDrawText right after that line, one line lower and right-aligned the same way (\a580r).
        const char* BuildLabel()
        {
            static std::string label = []
            {
                HMODULE self = nullptr;
                GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&BuildLabel), &self);
                wchar_t path[MAX_PATH];
                GetModuleFileNameW(self, path, MAX_PATH);
                std::wstring file = path;
                file = file.substr(0, file.find_last_of(L'\\') + 1) + L"kit-version.txt";
                std::ifstream in(file);
                std::string first;
                std::getline(in, first);
                while (!first.empty() && (first.back() == '\r' || first.back() == ' ')) first.pop_back();
                if (first.rfind("\xEF\xBB\xBF", 0) == 0) first.erase(0, 3);
                constexpr const char* kPrefix = "PhantomDust-pdnet ";
                if (first.rfind(kPrefix, 0) == 0) first.erase(0, strlen(kPrefix));
                return "pdnet " + (first.empty() ? std::string("development build") : first);
            }();
            return label.c_str();
        }

        NativeFn g_buttons = nullptr;
        char g_hintText[kSettingRows][128];
        const char* kConfirmBar = "\a2#Save \a580r\a3#Cancel";
        const char* kNormalBar = "\a8#Select \a7#Change \a580r\a2#Save \a3#Back";
        MenuCommandFn g_buttonBar = nullptr;

        void ShowButtonBar(const char* text)
        {
            if (!g_buttonBar) return;
            int64_t args[8] = { 2, reinterpret_cast<int64_t>(text), 0 };
            g_buttonBar(args);
        }

        void RefreshHints()
        {
            for (int r = 0; r < kSettingRows; ++r)
                strcpy_s(g_hintText[r], SettingHelp(r));
        }

        // The question is a pop-up window of its own: window 8 (the Cross Play detail box, unused now), set up the way the
        // script sets that box up (menu commands 33 init, 36 size, 37 position, 38 title, 40 lines) and drawn after the
        // Settings list (menu command 42 draws a window), its text with menu command 24 (absolute x, y).
        constexpr int kPopupWindow = 8, kPopupX = 150, kPopupY = 160, kPopupW = 340, kPopupH = 104;
        MenuCommandFn g_drawWindow = nullptr;

        int16_t* WindowAt(int64_t manager, int index) { return reinterpret_cast<int16_t*>(manager + 0x40 + index * 0x103c); }

        void OpenSavePopup()
        {
            int64_t manager = MenuManager();
            if (!manager) return;
            int16_t current = *reinterpret_cast<int16_t*>(manager + 0x10402); // the script's own window stays current
            auto cmd = [](uint64_t fn, int64_t a0, int64_t a1) { int64_t args[8] = { a0, a1 }; reinterpret_cast<MenuCommandFn>(At(fn))(args); };
            cmd(0x140207cd0, kPopupWindow, 2);
            cmd(0x140207d20, kPopupW, kPopupH);
            cmd(0x140207d50, kPopupX, kPopupY);
            cmd(0x140207d80, reinterpret_cast<int64_t>("SAVE SETTINGS"), 0);
            for (int i = 0; i < 3; ++i) cmd(0x140207e20, reinterpret_cast<int64_t>(""), 0);
            *reinterpret_cast<int16_t*>(manager + 0x10402) = current;
        }

        int64_t HookedDrawWindow(int64_t* args)
        {
            int64_t result = g_drawWindow(args);
            int64_t manager = MenuManager();
            if (!args || !g_confirmingSave || !InSettings() || !manager) return result;
            int16_t* list = WindowAt(manager, static_cast<int>(args[0]));
            if (args[0] == kPopupWindow || list[0] != 56 || list[2] != 470) return result; // after the Settings list only
            int64_t popup[8] = { kPopupWindow };
            g_drawWindow(popup);
            *reinterpret_cast<int64_t*>(manager + 0x10408) = reinterpret_cast<int64_t>(WindowAt(manager, kPopupWindow));
            auto text = [](int x, int y, const char* s) { int64_t a[8] = { x, y, reinterpret_cast<int64_t>(s) }; reinterpret_cast<MenuCommandFn>(At(0x140207870))(a); };
            text(kPopupX, kPopupY + 18, "\a340cSave all settings?");
            text(kPopupX, kPopupY + 44, "\a340c\a2#Save      \a3#Cancel");
            *reinterpret_cast<int64_t*>(manager + 0x10408) = 0;
            return result;
        }

        void SetConfirming(bool on)
        {
            g_confirmingSave = on;
            if (on) OpenSavePopup();
            ShowButtonBar(on ? kConfirmBar : kNormalBar);
        }

        int64_t HookedButtons(int64_t* args)
        {
            int64_t buttons = g_buttons(args);
            if (!InSettings() || (args && args[0] != 0)) return buttons;
            static uint64_t last = 0; // the script reads the buttons several times a frame: act once per press
            bool a = (buttons & 0x1010) != 0, b = (buttons & 0x2020) != 0;
            bool fresh = GetTickCount64() - last > 150;
            if (!g_confirmingSave)
            {
                if (a && fresh) // also while a name is being typed: saving counts it as entered
                {
                    last = GetTickCount64();
                    int64_t sound[2] = { 0, 0 };
                    reinterpret_cast<InputFn>(At(0x140261750))(sound);
                    SetConfirming(true);
                }
                return a ? buttons & ~0x1010ll : buttons;
            }
            if ((a || b) && fresh)
            {
                last = GetTickCount64();
                int64_t sound[2] = { a ? 0 : 1, 0 }; // the menu's select / back sounds
                reinterpret_cast<InputFn>(At(0x140261750))(sound);
                if (a) SaveAllSettings();
                SetConfirming(false);
                Log(L"gamepatch: settings: %s", a ? L"saved" : L"save cancelled");
            }
            return 0; // nothing else while asking
        }
        // The button bar (menu command 88 with 2, text): the Options screen's "Change / Back" gets "A Save".
        int64_t HookedButtonBar(int64_t* args)
        {
            const char* kSave = g_confirmingSave ? kConfirmBar : kNormalBar;
            char text[64];
            if (args && (InSettings() || GetTickCount64() - g_settingsOpenedAt < 500) && CopyString(reinterpret_cast<const char*>(args[1]), text, sizeof(text)) &&
                (strcmp(text, "\a7#Change \a580r\a3#Back") == 0 || strcmp(text, "\a8#Select \a7#Change \a580r\a3#Back") == 0))
            {
                int64_t changed[8];
                memcpy(changed, args, sizeof(changed));
                changed[1] = reinterpret_cast<int64_t>(kSave);
                return g_buttonBar(changed);
            }
            return g_buttonBar(args);
        }

        bool SettingsItems(const char* added)
        {
            bool rows = strcmp(added, "\a160wVoice Chat\f\a1#\a230c%s\f\a0#") == 0, hints = strcmp(added, "Set your voice chat status.") == 0;
            if (!rows && !hints) return false;
            if (rows)
            {
                g_settingsSaved = EditableConfig(); // what leaving unsaved goes back to
                g_settingsDirty = false;
                g_confirmingSave = false;
            }
            RefreshHints();
            RefreshSettingRows();
            for (int r = 0; r < kSettingRows; ++r)
            {
                int64_t item[8] = { reinterpret_cast<int64_t>(rows ? g_optionText[r] : g_hintText[r]) };
                g_addItem(item);
            }
            if (rows) Log(L"gamepatch: Settings: %d rows", static_cast<int>(kSettingRows));
            return true;
        }
        void SettingsScriptPatch(int menu, uint8_t* script, bool fresh)
        {
            if (menu != kSettingsMenu) return;
            int texts = RelabelScript(script, kSettingsTexts, std::size(kSettingsTexts)), patched = PatchSettingsScript(script);
            if (fresh) Log(L"gamepatch: Settings menu (was Xbox Live Options): %d text(s), A button %s", texts, patched ? L"changes settings" : L"left as it is (script not as expected)");
        }

        // Options > Sound (main menu; menu 4, option_en.ssb): a row under the game's Music / Sound Effects Volume, Aura
        // Particle Beeps (On / Off). The script builds the SOUND window (window 4) at 0x01350: HookedAddItem adds pdnet's
        // row after the Sound Effects row, and its hint after that row's hint. The window's left / right handler
        // (0x02AF4) and its row drawing (0x012E8) work on option 4 + cursor row through menu commands 66 (get) and 67
        // (set), in steps of 10. Option 6 is the Display window's otherwise, so only while the SOUND window is open
        // (script state SOUN, set with menu commands 12 and 6) it is pdnet's row: 66 answers 50, and 67 (40 or 60)
        // switches the setting and saves it.
        template <typename Fn> bool WrapCommand(uint64_t tableEntry, uint64_t expected, Fn hook, Fn& original, const wchar_t* name); // (below)
        constexpr int kOptionsMenu = 4;
        constexpr int kAuraOption = 6;
        std::atomic<bool> g_soundWindow{ false };
        char g_soundRowText[96];
        const char* kSoundHint = "Turn the beeping of the aura particles on or off.";

        bool InSoundWindow() { return g_soundWindow && CurrentMenu(MenuManager()) == kOptionsMenu; }

        void RefreshSoundRows()
        {
            sprintf_s(g_soundRowText, "\a190wAura Particle Beeps\f\a1#\a150c%s\f\a0#", GetConfig().auraBeeps ? "On" : "Off");
        }

        // Called for every item added: true when it was one of the two pdnet's row and hint follow (added here, then pdnet's).
        bool SoundItems(const char* added, int64_t* args, int64_t& result)
        {
            if (CurrentMenu(MenuManager()) != kOptionsMenu) return false;
            bool rows = strcmp(added, "\a190wSound Effects Volume\f\a1#\a150c%2d %s\f\a0#") == 0, hints = strcmp(added, "Set the sound effects volume.") == 0;
            if (!rows && !hints)
            {
                if (strcmp(added, "\a190wMusic Volume\f\a1#\a150c%2d %s\f\a0#") != 0 && strcmp(added, "Set the music volume.") != 0) g_soundWindow = false; // another window
                return false;
            }
            result = g_addItem(args);
            RefreshSoundRows();
            int64_t item[8] = { reinterpret_cast<int64_t>(rows ? g_soundRowText : kSoundHint) };
            g_addItem(item);
            if (rows)
            {
                g_soundWindow = true;
                Log(L"gamepatch: Options > Sound: Aura Particle Beeps row added");
            }
            return true;
        }

        MenuCommandFn g_getOption = nullptr, g_setOption = nullptr, g_setState = nullptr, g_setState12 = nullptr;
        int64_t HookedGetOption(int64_t* args)
        {
            if (args && args[0] == kAuraOption && InSoundWindow()) return 50;
            return g_getOption(args);
        }

        int64_t HookedSetOption(int64_t* args)
        {
            if (!args || args[0] != kAuraOption || !InSoundWindow()) return g_setOption(args);
            bool& on = EditableConfig().auraBeeps;
            on = !on;
            SaveSetting("aura_beeps", on ? "on" : "off");
            RefreshSoundRows();
            Log(L"gamepatch: Options > Sound: aura particle beeps %s", on ? L"on" : L"off");
            return 0;
        }

        // Menu commands 12 and 6 set the script's state ("SOUN" / "aSOUN" while the SOUND window is open).
        void NoteState(int64_t* args)
        {
            char state[16];
            if (!args || !CopyString(reinterpret_cast<const char*>(args[0]), state, sizeof(state))) return;
            size_t n = strlen(state);
            g_soundWindow = n >= 4 && strcmp(state + n - 4, "SOUN") == 0;
        }
        int64_t HookedSetState(int64_t* args) { NoteState(args); return g_setState(args); }
        int64_t HookedSetState12(int64_t* args) { NoteState(args); return g_setState12(args); }

        // Every game sound starts in FUN_140240870 (2D) or FUN_140240a00 (3D, at a position): bank (edx), cue (r8d),
        // sound in the cue (r9d). Both are hooked: a sound turned off in Options > Sound is not started (2D answers -1,
        // as when sound is off). sound_log=on logs each sound (with its caller) the first time it plays, and every 30 s
        // the ones that played often since.
        using SoundFn = int64_t (*)(int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t, int64_t);
        SoundFn g_sound2D = nullptr, g_sound3D = nullptr;

        uint64_t GhidraAddress(void* code) { return static_cast<uint64_t>(static_cast<uint8_t*>(code) - Image()) + 0x140000000ull; }

        // The aura beep: FUN_14024de20 raises a player's aura towards its maximum and, each time the whole number goes
        // up (own player only), plays 2D bank 0 cue 5 (through FUN_140276330, which jumps into the 2D start, so the
        // caller seen is its call at 0x14024DF22). Found 2026-10-27 after bank 1 cues 4 and 1 (the stage's sound
        // emitters, FUN_1402214c0) were tried and the beep stayed.
        bool AuraBeep(bool positional, int64_t bank, int64_t cue, uint64_t caller) { return !positional && bank == 0 && cue == 5 && caller == 0x14024DF22; }

        void LogSound(bool positional, int64_t bank, int64_t cue, int64_t sound, uint64_t caller, bool auraOff)
        {
            static std::mutex lock;
            static std::map<uint64_t, uint32_t> counts;
            static uint64_t summaryAt = GetTickCount64();
            uint64_t key = (positional ? 1ull << 63 : 0) | (static_cast<uint64_t>(bank) & 0xff) << 48 | (static_cast<uint64_t>(cue) & 0xffff) << 32 |
                           (static_cast<uint64_t>(sound) & 0xffff) << 16 | ((caller - 0x140000000ull) & 0xffff);
            std::lock_guard guard(lock);
            if (counts.find(key) == counts.end())
                Log(L"sound: %s bank %lld cue %lld sound %lld from 0x%llX (menu %d)%s", positional ? L"3D" : L"2D", bank, cue, sound, caller, CurrentMenu(MenuManager()),
                    auraOff && AuraBeep(positional, bank, cue, caller) ? L" (muted: aura beeps off)" : L"");
            ++counts[key];
            if (GetTickCount64() - summaryAt < 30000) return;
            summaryAt = GetTickCount64();
            for (auto& [k, n] : counts)
                if (n >= 5) Log(L"sound: %s bank %llu cue %llu sound %llu played %u times in 30 s", (k >> 63) ? L"3D" : L"2D", (k >> 48) & 0xff, (k >> 32) & 0xffff, (k >> 16) & 0xffff, n);
            for (auto& [k, n] : counts) n = 0;
        }

        bool SoundOff(bool positional, int64_t bank, int64_t cue, int64_t sound, uint64_t caller)
        {
            const Config& c = GetConfig();
            if (c.soundLog) LogSound(positional, static_cast<int32_t>(bank), static_cast<int32_t>(cue), static_cast<int32_t>(sound), caller, !c.auraBeeps);
            return !c.auraBeeps && AuraBeep(positional, static_cast<int32_t>(bank), static_cast<int32_t>(cue), caller);
        }

        int64_t HookedSound2D(int64_t a, int64_t bank, int64_t cue, int64_t sound, int64_t e, int64_t f, int64_t g, int64_t h)
        {
            if (SoundOff(false, bank, cue, sound, GhidraAddress(_ReturnAddress()))) return -1;
            return g_sound2D(a, bank, cue, sound, e, f, g, h);
        }

        int64_t HookedSound3D(int64_t a, int64_t bank, int64_t cue, int64_t sound, int64_t e, int64_t f, int64_t g, int64_t h)
        {
            uint64_t caller = GhidraAddress(_ReturnAddress());
            // Most 3D sounds come through FUN_140276330 (sub rsp,58h, then the call): name its caller instead.
            if (caller == 0x140276399) caller = GhidraAddress(reinterpret_cast<void**>(_AddressOfReturnAddress())[1 + 0x58 / 8]);
            if (SoundOff(true, bank, cue, sound, caller)) return 0;
            return g_sound3D(a, bank, cue, sound, e, f, g, h);
        }

        void SoundOptions()
        {
            // FUN_140240870 starts with sub rsp,68h / mov rax,[rip+...] (the stack cookie at 0x1403d40f8): the
            // trampoline loads the cookie from its absolute address instead (mov rax,[imm64]).
            std::vector<uint8_t> start2D = { 0x48, 0x83, 0xEC, 0x68, 0x48, 0x8B, 0x05, 0x7D, 0x38, 0x19, 0x00 };
            std::vector<uint8_t> replay2D = { 0x48, 0x83, 0xEC, 0x68, 0x48, 0xA1 };
            Put64(replay2D, reinterpret_cast<uint64_t>(At(0x1403d40f8)));
            std::vector<uint8_t> start3D = { 0x40, 0x55, 0x41, 0x56, 0x41, 0x57 }; // push rbp / push r14 / push r15
            g_sound2D = reinterpret_cast<SoundFn>(InlineHookBytes(0x140240870, start2D, replay2D, reinterpret_cast<void*>(&HookedSound2D), L"sound (2D)"));
            g_sound3D = reinterpret_cast<SoundFn>(InlineHookBytes(0x140240a00, start3D, start3D, reinterpret_cast<void*>(&HookedSound3D), L"sound (3D)"));
            bool rows = WrapCommand<MenuCommandFn>(0x1403eefe0 + 66 * 8, 0x140208260, &HookedGetOption, g_getOption, L"Options > Sound rows (get)") &&
                        WrapCommand<MenuCommandFn>(0x1403eefe0 + 67 * 8, 0x140208280, &HookedSetOption, g_setOption, L"Options > Sound rows (set)") &&
                        WrapCommand<MenuCommandFn>(0x1403eefe0 + 6 * 8, 0x140207180, &HookedSetState, g_setState, L"Options > Sound state") &&
                        WrapCommand<MenuCommandFn>(0x1403eefe0 + 12 * 8, 0x140207220, &HookedSetState12, g_setState12, L"Options > Sound state (12)");
            Log(L"gamepatch: Options > Sound: row %s, sounds %s (aura beeps %s%s)", rows ? L"ready" : L"not added",
                g_sound2D && g_sound3D ? L"hooked" : L"NOT hooked", GetConfig().auraBeeps ? L"on" : L"off", GetConfig().soundLog ? L", sound_log on" : L"");
        }

        // pdlaunch keeps the game at splash_volume while the title screen or intro is on (menu 7 playdatasel, 80, or no
        // menu yet): LocalState\pdnet\past-title.txt exists while any other menu is (normally the main menu, 8), and is
        // deleted again when the player backs out to the title. pdlaunch deletes it as the game starts.
        constexpr int kTitleMenu = 7, kTitleAttractMenu = 80;
        void SignalPastTitle()
        {
            std::thread([]
            {
                std::wstring path = LocalStatePath() + L"\\pdnet\\past-title.txt";
                bool past = false;
                for (;;)
                {
                    Sleep(100);
                    int menu = CurrentMenu(MenuManager());
                    bool now = !(menu < 2 || menu == kTitleMenu || menu == kTitleAttractMenu);
                    if (now == past) continue;
                    past = now;
                    if (past)
                    {
                        CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
                        if (FILE* f = nullptr; !_wfopen_s(&f, path.c_str(), L"w") && f)
                        {
                            fprintf(f, "%d\n", menu);
                            fclose(f);
                        }
                    }
                    else DeleteFileW(path.c_str());
                    Log(L"gamepatch: %s (menu %d): start-up volume %s", past ? L"past the title screen" : L"back on the title screen", menu, past ? L"ends" : L"again");
                }
            }).detach();
        }

        bool RatedRuleText(int64_t* args, char* out, size_t size)
        {
            char format[64], rule[48];
            if (!CopyString(reinterpret_cast<const char*>(args[2]), format, sizeof(format)) || !strstr(format, "Game Rule")) return false;
            if (!CopyString(reinterpret_cast<const char*>(args[3]), rule, sizeof(rule)) || !rule[0]) return false;
            int64_t cursor[2] = { 1, 9 }; // the script pushes 9 then 1: the last push is the first argument
            int64_t row = reinterpret_cast<MenuCommandFn>(*reinterpret_cast<void**>(At(0x1403eefe0) + 55 * 8))(cursor);
            int rated = row >= 0 ? scores::SearchResultRated(static_cast<size_t>(row)) : -1;
            if (rated < 0) return false;
            sprintf_s(out, size, "%s (%s)", rule, rated ? "Rated" : "Unrated");
            return true;
        }

        int64_t HookedDrawText(int64_t* args)
        {
            if (args && GetConfig().ratedLobbies && CurrentMenu(MenuManager()) == kSessionListMenu)
            {
                static char text[96];
                if (RatedRuleText(args, text, sizeof(text)))
                {
                    int64_t changed[8];
                    memcpy(changed, args, sizeof(changed));
                    changed[3] = reinterpret_cast<int64_t>(text);
                    return g_drawText(changed);
                }
                return g_drawText(args);
            }
            int64_t result = g_drawText(args);
            char format[24];
            if (args && CopyString(reinterpret_cast<const char*>(args[2]), format, sizeof(format)) && strncmp(format, "\a580rSkill Update", 17) == 0)
            {
                static std::string line = std::string("\a580r") + BuildLabel();
                int64_t label[8] = { args[0], args[1] + 20, reinterpret_cast<int64_t>(line.c_str()) };
                g_drawText(label);
                return result;
            }
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
        Log(L"menu: invite button in the lobby - opening the Arsenal editor (red hand on until back in the lobby)");
        g_editorFromLobby = true;
        g_lobbyEndedInEditor = false;
        SetLobbyAvailable(false); // like the lobby's own menus: the match can't start while this player is in the editor
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
            QuickJoinLabels();
            if (GetConfig().shopMaxOut) ShopMaxOut();
            if (GetConfig().ratedLobbies) RatedLobbies();
            if (GetConfig().ratedLobbies && !g_drawText) WrapCommand<MenuCommandFn>(0x1403eefe0 + 23 * 8, 0x140207840, &HookedDrawText, g_drawText, L"session info rated text");
            if (GetConfig().ratedLobbies || GetConfig().scoreboard) MenuTextPatches(GetConfig().ratedLobbies, GetConfig().scoreboard);
            // The in-game Settings menu (what was Xbox Live Options): left / right on pdnet's rows.
            WrapCommand<NativeFn>(0x1403ddde0 + 3 * 8, 0x1400bde50, &HookedSettingFlag, g_settingFlag, L"no Cross Play option");
            WrapCommand<MenuCommandFn>(0x1403eefe0 + 37 * 8, 0x140207d50, &HookedPlaceWindow, g_placeWindow, L"settings window position");
            WrapCommand<MenuCommandFn>(0x1403eefe0 + 88 * 8, 0x140207650, &HookedButtonBar, g_buttonBar, L"settings save button");
            WrapCommand<MenuCommandFn>(0x1403eefe0 + 42 * 8, 0x140207f00, &HookedDrawWindow, g_drawWindow, L"settings save pop-up");
            WrapCommand<NativeFn>(0x1403fb108, 0x1402168a0, &HookedButtons, g_buttons, L"settings save confirmation");
            if (WrapCommand<SessionCommandFn>(0x1403f1150 + 154 * 8, 0x140214d60, &HookedOptionChanged, g_optionChanged, L"settings rows"))
                Log(L"gamepatch: in-game Settings ready (multiplayer menu > Settings)");
            if (WrapCommand<MenuCommandFn>(0x1403eefe0 + 40 * 8, 0x140207e20, &HookedAddItem, g_addItem, L"menu item names"))
                Log(L"gamepatch: menu items renamed as they are added (Quick Join, Lobby Browser%s)", GetConfig().scoreboard ? L", Global scoreboards only" : L"");
            SoundOptions(); // Options > Sound: Aura Particle Beeps
            SignalPastTitle(); // pdlaunch's start-up volume lasts until then
            bool arsenal = GetConfig().lobbyArsenalEditor; // the Arsenal Lab button for clients too
            if (GetConfig().lobbyGamertagOption || micLabel || arsenal) LobbyScriptPatches(GetConfig().lobbyGamertagOption, micLabel, arsenal);
        });
    }

    // FUN_1400bd7f0 loads it from progress.dat (above 999999: not loaded yet, the game then uses its default);
    // after a match FUN_1401835c0 adds the change to the current rating (0x14051c0b0, kept within 0-999999) and
    // FUN_1400bf1f0 copies it here and reports it as the Hero_MP_Ranking event.
    void NoteMatchWin() { g_matchWon = true; }

    void LobbyEnded()
    {
        if (!g_editorFromLobby || g_lobbyEndedInEditor.exchange(true)) return;
        Log(L"menu: the lobby ended while its Arsenal editor is open - leaving the editor goes out of the lobby");
    }

    int SavedRating()
    {
        int64_t rating = *reinterpret_cast<int64_t*>(At(0x1405b1370));
        return rating >= 0 && rating <= 999999 ? static_cast<int>(rating) : -1;
    }

    // The save (0x1404bec10) holds two profiles; +0x33a0 points to the active one, +0x3fb8 is the multiplayer one.
    bool MultiplayerProfile()
    {
        uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
        return save && *reinterpret_cast<uint8_t**>(save + 0x33a0) == save + 0x3fb8;
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

    // As FUN_1401831f0 (session command 144) works it out, read-only: rule N needs the arsenal flags in the rule
    // table 0x1402dbc78[N] (0 = no arsenal needed: Quick); the multiplayer arsenals are up to 16 entries of 100 bytes
    // at *(save + 0x10a*0x38) (save = DAT_1404bec10), an entry with byte +4 == 0 ends the list, flags word at +0x52.
    uint32_t JoinableRules()
    {
        __try
        {
            auto table = reinterpret_cast<const int32_t*>(At(0x1402dbc78));
            uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
            uint8_t* arsenals = save ? *reinterpret_cast<uint8_t**>(save + 0x10a * 0x38) : nullptr;
            uint32_t mask = 0;
            for (int rule = 0; rule < 12; ++rule)
            {
                uint32_t need = static_cast<uint32_t>(table[rule]);
                bool ok = need == 0;
                for (int i = 0; !ok && arsenals && i < 16; ++i)
                {
                    uint8_t* arsenal = arsenals + i * 100;
                    if (!arsenal[4]) break;
                    ok = (*reinterpret_cast<uint16_t*>(arsenal + 0x52) & need) != 0;
                }
                if (ok) mask |= 1u << rule;
            }
            return mask;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return 0xfff;
        }
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
