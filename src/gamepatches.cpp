#include "pch.h"
#include "gamepatches.h"
#include "config.h"
#include "lobbies.h"
#include "scores.h"
#include "discord.h"
#include "log.h"
#include "rebirth_word.h"
#include "gamepads.h"
#include "labtests.h"
#include "film.h"
#include "pdpl.h"
#include "mods.h"
#include "variants.h"
#include "p2p.h"
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

        std::atomic<bool> g_theatre{ false }; // the Settings screen (menu 68) is showing the Theatre's film list
        std::atomic<bool> g_modsList{ false }; // the Settings screen is showing the Mods list (Conceptual; was Quick Join)
        void LeaveModList(); // (below)
        std::atomic<uint8_t> g_bodyTags{ 0 }; // nametags on dead bodies wanted (the lobby host's setting)
        // What the game's nametag code reads (routines below), kept current by NametagColours: dead bodies keep their
        // tag only in Tag Team, and each other player's tag colour by match slot (ally / enemy, both ARGB twice).
        std::atomic<uint8_t> g_bodyTagsNow{ 0 };
        std::atomic<uint8_t> g_slotGone[4]{}; // per match slot: that player left (no nametag), set by NametagColours
        std::atomic<bool> g_testBattle{ false }; // Lab > Test Arsenal's battle is on (OpenTestArsenal); its AI is no network player
        bool AiBattle() { return g_testBattle; } // no left players, wins or rating changes then
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

        // Nametags are always shown in a match. The battle HUD (DAT_1404bec00) has a "names hidden" switch at +0x5c: the
        // match starts with it at 1 (FUN_140150130), a button (pad bit 9) flips it (FUN_140150640), and FUN_140150c30
        // skips every nametag while it is 1:   cmp dword ptr [r14+0x5c],1 / jz skip   (jz at 0x140151165)
        // The jz is removed, and the button's store (mov [rbx+0x5c],eax at 0x140150767) becomes nops, so it does nothing.
        void NametagsAlwaysOn()
        {
            const std::vector<uint8_t> skip = { 0x0f, 0x84, 0xe3, 0x01, 0x00, 0x00 };
            const std::vector<uint8_t> toggle = { 0x89, 0x43, 0x5c };
            uint8_t* skipSite = At(0x140151165);
            uint8_t* toggleSite = At(0x140150767);
            if (memcmp(skipSite, skip.data(), skip.size()) != 0 || memcmp(toggleSite, toggle.data(), toggle.size()) != 0)
            {
                Log(L"gamepatch: nametags always on skipped - the game code is not what this pdnet expects");
                return;
            }
            if (!Write(skipSite, std::vector<uint8_t>(skip.size(), 0x90)) || !Write(toggleSite, std::vector<uint8_t>(toggle.size(), 0x90)))
            {
                Log(L"gamepatch: nametags always on failed (VirtualProtect error %lu)", GetLastError());
                return;
            }
            Log(L"gamepatch: nametags always on (the nametag button does nothing)");
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
            // Only a player who was connected during this match can leave it: AI players (Test Arsenal, the debug menu's
            // battles, where every nametag was hidden; 2026-11-12) are never network players.
            static bool seen[4] = {};
            for (int s = 0; s < 4; ++s)
            {
                if (!match || !v.matchName[s][0]) seen[s] = false;
                else if (present[s]) seen[s] = true;
                uint8_t gone = match && !AiBattle() && seen[s] && !present[s] ? 1 : 0;
                if (g_slotGone[s].exchange(gone) != gone && gone) Log(L"gamepatch: %S left the match - nametag hidden", v.matchName[s]);
            }
        }

        // AI players (the debug menu's battles) have no name at 0x1404bca30 + i*0x41, so their nametag drew nothing (the
        // user, 2026-11-12; log: "0 enemy, 1 -, 2 -, 3 -"). During a match, a slot with a player (status record
        // DAT_1404bc608[i], count 0x1404bc628) and no name is named "CPU <slot + 1>". Online players always have names.
        void NameAiPlayers()
        {
            __try
            {
                // Only once the match runs (game state 6): by then the game has written every real player's name. And
                // never in the story: its towns hold the NPCs in these slots with no name, and Experimental 155 naming
                // them made every NPC disappear (2026-11-12); FUN_1401ca1e0 builds the characters' names from them.
                // The names pdnet wrote are cleared again once the match is over (they stayed in the slots otherwise).
                static bool named[4] = {};
                bool running = *reinterpret_cast<int32_t*>(At(0x1404bc950)) == 6 && MultiplayerProfile();
                for (int i = 0; i < 4; ++i)
                {
                    char* name = reinterpret_cast<char*>(At(0x1404bca30) + i * 0x41);
                    char ours[16];
                    sprintf_s(ours, "CPU %d", i + 1);
                    if (!running)
                    {
                        if (named[i] && strcmp(name, ours) == 0) name[0] = 0;
                        named[i] = false;
                        continue;
                    }
                    if (named[i] || i >= *reinterpret_cast<int32_t*>(At(0x1404bc628))) continue;
                    if (name[0] || !*reinterpret_cast<uint64_t*>(At(0x1404bc608) + i * 8)) continue;
                    strcpy_s(name, 0x41, ours);
                    named[i] = true;
                    Log(L"gamepatch: match slot %d has no name (an AI player) - nametag reads %S", i, name);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
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
                    // Read every pass: the in-game Settings menu can change them (White is one of the colours).
                    uint64_t ally = twice(GetConfig().allyNametagColor), enemy = twice(GetConfig().enemyNametagColor);
                    g_deadColour = twice(GetConfig().deadNametagColor);
                    NameAiPlayers();
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
        // Menu indexes (table DAT_1403fa3d0, 0x18 per entry): arsenalbuy 30, arsenalsell 31, livelobby 44,
        // laboratory 45, labostock 63, arsenaledit 64. The stack nests: 36 -> 45 -> 64, back, back.
        // Lobby Arsenal Lab: the lobby's invite button (pdnet's ShowGameInviteUIAsync) opens the Laboratory on top of
        // livelobby (it opened arsenaledit directly until 2026-10-29; the Laboratory has Buy / Sell Arsenal Case now).
        // While it is open from there, leaving the Laboratory's menus for any other menu becomes a "back", so it returns
        // to the lobby with the lobby's state restored, like the lobby's own Change Arsenal.
        constexpr int kSettingsMenuId = 68; // Settings (liveoption), also shown as the Theatre and as the Mods list
        constexpr int kLobbyMenu = 44, kLabMenu = 45, kStockMenu = 63, kEditorMenu = 64, kCaseBuyMenu = 30, kCaseSellMenu = 31;
        bool InLabMenus(int menu) { return menu == kLabMenu || menu == kStockMenu || menu == kEditorMenu || menu == kCaseBuyMenu || menu == kCaseSellMenu; }
        // Lab > Test Arsenal (OpenTestArsenal) runs the scoreboard battle: livedbtl 74 (arsenal choice, then the battle),
        // livearsel 67, compe_res 78 (results, then the multiplayer menu 36). Its back to the scoreboard (41) goes to the
        // Lab instead, and wins in it don't count for the EOS scoreboard (TestBattle).
        constexpr int kScoreBattleMenu = 74, kScoreboardMenu = 41, kMultiplayerMenu = 36;
        // livedbtlres (75) comes after a scoreboard battle: Xbox Live player feedback on the opponent ("Send this
        // feedback?", Cheating...), dead without Xbox Live and removed: a Test Arsenal battle goes back to the Lab,
        // any other to the scoreboard, where the feedback screen would have gone.
        constexpr int kFeedbackMenu = 75;
        // A Lab opened after a test battle sits on top of the battle (menu 1), and one opened when a test is cancelled on
        // top of the opponent's arsenal choice: a plain back would return there (a black screen, Experimental 87), so its
        // back opens the multiplayer menu instead.
        std::atomic<bool> g_labAfterTest{ false };
        // The Lab's scripts read menu variables 0, 1 and 2 (command 14; DAT_140533c10, ints) as they start: variable 0 & 0xff
        // picks the save's list (g5 #111, +0xb50), and only lists 0 and 1 exist (FUN_14015a520). The multiplayer menu sets
        // all three to 0 just before opening the Lab (livemain_en.ssb 0x175c); when pdnet opens it, nothing did. A battle
        // leaves variable 0 at 2 (command_en.ssb), so View Stock List copied from an empty list and crashed
        // (PDUWP+0x15a257, FUN_14015a230 with list 2; Experimental 99 and 100; the dump shows variable 0 = 2).
        int32_t* MenuVariables() { return reinterpret_cast<int32_t*>(At(0x140533c10)); }
        void ResetLabVariables() { memset(MenuVariables(), 0, 3 * sizeof(int32_t)); }
        // Test Arsenal asks, in order: the player's character and arsenal, the AI's character and arsenal, the map; then
        // opens livedbtl (74) with its menu parameter 0 = 1, which starts the battle. Each step is on the game's own menu and
        // ends with a back (HookedPopMenu), picked or not, so the next step opens from there:
        // - livecharasel (46; g4 #571 stores the pick in 0x140527bc0): opened with that at -1. Its back leaves the session
        //   unless menu variable 0 is 1, as the lobby sets it before opening it.
        // - livearsel (67) in scoreboard mode (its menu parameter 0 = 1) stores a pick with g4 #575 (0x1403de844, the
        //   arsenal the battle gives the player) and sets livedbtl's parameter 0 to 1, or to 2 when cancelled.
        // - livestagesel (47; g4 #576 stores the pick in 0x14051775c, the map id in 0x140527bc8 and a random layout in
        //   0x140527c54): opened with that at -1.
        // The scoreboard battle (g4 #661 = FUN_140214770) has map 2, layout 0 and character 0 built in, so HookedTestStart
        // passes the picks to its FUN_140128990 (the match setup, 0x48 per player: the character at +8; online, the same field).
        constexpr int kArsenalPickMenu = 67, kCharacterMenu = 46, kStageMenu = 47;
        std::atomic<bool> g_forceBack{ false }; // the character select backs out at its next input label (the Advanced list, HookedSetState)
        // In a lobby the B event goes back to the lobby setup (menu 48) and the lobby sends the player to the character
        // select again (the user's test, 2026-11-21: a loop). There the Advanced list confirms the game's own way: the A
        // event on the grid (the script opens its COLORS window, 2), then A on its first row; the store (g4 #571) then
        // gets pdnet's value (g_pendingPick) and the script finishes as after any pick (session cmd 35, back to the lobby).
        enum ForceConfirm { kConfirmNone, kConfirmGrid, kConfirmWaitColours, kConfirmColours };
        std::atomic<int> g_forceConfirm{ kConfirmNone };
        std::atomic<ULONGLONG> g_forceConfirmSince{ 0 };
        std::atomic<int> g_pendingPick{ -1 }; // the value the next store (g4 #571) writes instead of the script's
        enum TestPick { kPickingNone, kPickingCharacter, kPickingArsenal, kPickingOpponentCharacter, kPickingOpponentArsenal, kPickingStage };
        std::atomic<int> g_testPicking{ kPickingNone };
        std::atomic<int> g_testStage{ -1 }, g_testLayout{ 0 }, g_testCharacter{ -1 }, g_testOpponentCharacter{ -1 }, g_testArsenal{ 0 };
        std::atomic<ULONGLONG> g_matchLoadingSince{ 0 }; // when the load into the current match began (BackButtonTick); 0: none
        // Menu command 88, line 1: the heading. These menus set none of their own, so the last one stays on screen.
        void SetMenuHeading(const char* text)
        {
            int64_t heading[8] = { 1, reinterpret_cast<int64_t>(text), 0 };
            reinterpret_cast<int64_t (*)(int64_t*)>(At(0x140207650))(heading);
        }
        // Those menus draw from the multiplayer texture file: "MNET" in slot 3 (window_chara and so on), which the
        // multiplayer menu frees right before it opens the Lab (livemain_en.ssb 0x16e4) and loads again when it comes back
        // (0x1a84). Without it the character screen is only its text (Experimental 111). So it's loaded as the scripts do
        // (menu command 16 until it starts, 17 until it's done; each is one try). livedbtl frees it again before the battle.
        constexpr int kMultiplayerTextures = 3;
        void LoadMultiplayerTextures()
        {
            auto load = reinterpret_cast<int64_t (*)(int64_t*)>(At(0x1402076b0));
            auto done = reinterpret_cast<int64_t (*)(int64_t*)>(At(0x1402076d0));
            int64_t args[8] = { kMultiplayerTextures, reinterpret_cast<int64_t>("MNET") };
            ULONGLONG start = GetTickCount64();
            while (!load(args))
            {
                if (GetTickCount64() - start > 3000) { Log(L"gamepatch: Test Arsenal: MNET textures not loaded"); return; }
                Sleep(5);
            }
            while (!done(args))
            {
                if (GetTickCount64() - start > 3000) { Log(L"gamepatch: Test Arsenal: MNET textures not ready after 3 s"); return; }
                Sleep(5);
            }
            Log(L"gamepatch: Test Arsenal: MNET textures loaded (%llu ms)", GetTickCount64() - start);
        }
        void FreeMultiplayerTextures()
        {
            int64_t args[8] = { kMultiplayerTextures };
            reinterpret_cast<int64_t (*)(int64_t*)>(At(0x1402077d0))(args);
        }
        int16_t& MenuParam(int menu, int slot) { return *reinterpret_cast<int16_t*>(At(0x1403fa3d8 + menu * 0x18 + slot * 2)); }
        bool CopyArsenalToOpponent(int index); // (below)
        using PushMenuFn = int (*)(int64_t manager, int menu, char force);
        using PopMenuFn = int (*)();
        PushMenuFn g_pushMenu = nullptr; // the original functions (trampolines)
        PopMenuFn g_popMenu = nullptr;
        std::atomic<bool> g_editorFromLobby{ false };
        int32_t g_lobbyVariable2 = 0; // menu variable 2 as the lobby had it when its Arsenal Lab button was used
        void RestoreLobbyVariable2() { MenuVariables()[2] = g_lobbyVariable2; }
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
            // The Help & Support button (the Phantom Dust page on xbox.com) opens the community Discord (gamesave.cpp).
            { "help_main", "HELP & SUPPORT", "COMMUNITY" },
            { "help", "Help & Support", "Community" },
            { "help_hint", "Go to the Phantom Dust page on Xbox.com.", "Join the Phantom Dust Rebirth community on Discord." },
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
        // Back in the lobby from its Arsenal Lab: available again, but only once the skill preview's town session (see
        // SkillPreviewScene) has fully closed, so the host can't start the match on top of it.
        void LobbyAvailableAfterPreview();

        void ClearFilmVariants(const char* why); // (below, with the variants)
        int HookedPushMenu(int64_t manager, int menu, char force)
        {
            int current = CurrentMenu(manager);
            if (!g_testBattle) ClearFilmVariants("the film's battle is over");
            bool theatre = false, mods = false;
            if (menu == 40 && current == 36) // Theater (livetrade 40 from livemain 36): the film list, not Trade
            {
                menu = 68; // the Settings screen, as the Theatre
                theatre = true;
                Log(L"theatre: opened (in place of Trade)");
            }
            // Mods (was Quick Join): its pick opened the session list (43) straight from livemain (36); the Lobby Browser goes
            // there through its search options (38) instead, so only Quick Join's pick is turned into the Mods list.
            if (menu == 43 && current == 36)
            {
                menu = 68; // the Settings screen, as the Mods list
                mods = true;
                Log(L"mods: list opened (in place of Quick Join)");
                mods::ListOpened(); // its script also starts Quick Join's search: that one finds nothing (MultiplayerService)
            }
            if (menu == 68)
            {
                g_theatre = theatre;
                g_modsList = mods;
                g_settingsOpenedAt = GetTickCount64();
            }
            RenameStrings(); // cheap; covers string tables loaded after start-up
            if (menu != current) Log(L"menu: open %d (from %d)", menu, current);
            if (GetConfig().debugMenu && menu == 5 && current == 8)
            {
                Log(L"menu: debug_menu=on - Split Screen opens the debug menu (menu 2) instead");
                return g_pushMenu(manager, 2, force);
            }
            if (menu == kFeedbackMenu)
            {
                bool test = g_testBattle.exchange(false);
                int to = test ? kLabMenu : kScoreboardMenu;
                if (test)
                {
                    g_labAfterTest = true;
                    ResetLabVariables();
                }
                Log(L"menu: player feedback screen skipped - menu %d instead", to);
                return g_pushMenu(manager, to, force);
            }
            if (g_testBattle && current == kScoreBattleMenu && menu == kScoreboardMenu)
            {
                g_testBattle = false;
                g_labAfterTest = true; // 74 sits on the opponent's arsenal choice: the Lab's back goes to the multiplayer menu
                Log(L"menu: leaving Test Arsenal - back to the Lab instead of the scoreboard");
                ResetLabVariables();
                return g_pushMenu(manager, kLabMenu, force);

            }
            if (g_testBattle && (menu == kMultiplayerMenu || menu == kLabMenu))
            {
                g_testBattle = false;
                Log(L"gamepatch: Test Arsenal over");
            }
            if (g_editorFromLobby && InLabMenus(current) && !InLabMenus(menu))
            {
                g_editorFromLobby = false;
                if (g_lobbyEndedInEditor.exchange(false))
                {
                    Log(L"menu: leaving the Arsenal Lab - the lobby ended meanwhile, so out the lobby's way (menu %d)", kLobbyLostMenu);
                    return g_pushMenu(manager, kLobbyLostMenu, force);
                }
                Log(L"menu: leaving the Arsenal Lab opened from the lobby - back to the lobby instead of menu %d", menu);
                RestoreLobbyVariable2();
                int result = g_popMenu();
                LobbyAvailableAfterPreview();
                return result;
            }
            return g_pushMenu(manager, menu, force);
        }

        int HookedPopMenu()
        {
            int64_t manager = MenuManager();
            int current = CurrentMenu(manager);
            Log(L"menu: back (from %d)", current);
            if (current == kSettingsMenuId && g_modsList.exchange(false)) LeaveModList();
            int step = g_testPicking;
            if (step != kPickingNone && current == (step == kPickingStage ? kStageMenu : step == kPickingCharacter || step == kPickingOpponentCharacter ? kCharacterMenu : kArsenalPickMenu))
            {
                *At(0x1404bbc34) = 0;
                bool arsenal = current == kArsenalPickMenu;
                int picked = arsenal ? (MenuParam(kScoreBattleMenu, 0) == 1 ? *reinterpret_cast<int32_t*>(At(0x1403de844)) : -1)
                                     : *reinterpret_cast<int32_t*>(At(current == kStageMenu ? 0x14051775c : 0x140527bc0));
                if (picked < 0)
                {
                    g_testPicking = kPickingNone;
                    g_testBattle = false;
                    ResetLabVariables();
                    FreeMultiplayerTextures();
                    Log(L"gamepatch: Test Arsenal: nothing picked on menu %d - back to the Lab", current);
                    if (step == kPickingCharacter) return g_popMenu(); // right on top of the Lab
                    g_labAfterTest = true;
                    return g_pushMenu(manager, kLabMenu, 0);
                }
                auto openCharacter = [&](int next)
                {
                    *reinterpret_cast<int32_t*>(At(0x140527bc0)) = -1;
                    MenuVariables()[0] = 1;
                    g_testPicking = next;
                    return g_pushMenu(manager, kCharacterMenu, 0);
                };
                auto openArsenal = [&](int next, const char* heading)
                {
                    MenuVariables()[0] = 0;
                    MenuParam(kArsenalPickMenu, 0) = 1; // livearsel in scoreboard mode
                    MenuParam(kScoreBattleMenu, 0) = 0;
                    SetMenuHeading(heading);
                    g_testPicking = next;
                    return g_pushMenu(manager, kArsenalPickMenu, 0);
                };
                switch (step)
                {
                case kPickingCharacter:
                    g_testCharacter = picked;
                    Log(L"gamepatch: Test Arsenal: character %d", picked);
                    return openArsenal(kPickingArsenal, "YOUR ARSENAL");
                case kPickingArsenal:
                    g_testArsenal = picked;
                    Log(L"gamepatch: Test Arsenal: arsenal %d", picked);
                    return openCharacter(kPickingOpponentCharacter);
                case kPickingOpponentCharacter:
                    g_testOpponentCharacter = picked;
                    Log(L"gamepatch: Test Arsenal: opponent's character %d", picked);
                    return openArsenal(kPickingOpponentArsenal, "AI OPPONENT'S ARSENAL");
                case kPickingOpponentArsenal:
                    CopyArsenalToOpponent(picked);
                    *reinterpret_cast<int32_t*>(At(0x1403de844)) = g_testArsenal; // the battle gives the player this one
                    *reinterpret_cast<int32_t*>(At(0x14051775c)) = -1;
                    MenuVariables()[0] = 0;
                    SetMenuHeading("LEVEL SELECT");
                    g_testPicking = kPickingStage;
                    return g_pushMenu(manager, kStageMenu, 0);
                default: // kPickingStage
                    g_testStage = *reinterpret_cast<int32_t*>(At(0x140527bc8));
                    g_testLayout = *reinterpret_cast<int32_t*>(At(0x140527c54));
                    Log(L"gamepatch: Test Arsenal: map %d, layout %d - starting", g_testStage.load(), g_testLayout.load());
                    g_testPicking = kPickingNone;
                    ResetLabVariables();
                    MenuParam(kScoreBattleMenu, 0) = 1; // livedbtl: start the battle
                    return g_pushMenu(manager, kScoreBattleMenu, 0);
                }
            }
            if (current == kLabMenu && g_labAfterTest.exchange(false))
            {
                Log(L"menu: back from the Lab after Test Arsenal - to the multiplayer menu (the battle is under it)");
                *At(0x1404bbc34) = 0;
                return g_pushMenu(manager, kMultiplayerMenu, 0);
            }
            if (current == kLabMenu && g_editorFromLobby.exchange(false)) // the Laboratory's back: the lobby is under it
            {
                if (g_lobbyEndedInEditor.exchange(false))
                {
                    Log(L"menu: leaving the Arsenal Lab - the lobby ended meanwhile, so out the lobby's way (menu %d)", kLobbyLostMenu);
                    return g_pushMenu(manager, kLobbyLostMenu, 0);
                }
                RestoreLobbyVariable2();
                int result = g_popMenu();
                LobbyAvailableAfterPreview(); // back in the lobby: no red hand (once a preview's town session has closed)
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

        // menu_debug: the configured menu's script commands (menu_debug=on: the lobby, 44), each command with its first
        // four arguments (8-byte slots, as the script passes them) and its result, once per distinct call (many run every
        // frame), at most 5000 lines.
        void LogLobbyCommand(uint32_t id, int64_t result, const int32_t* args)
        {
            static std::atomic<int> logged{ 0 };
            uint32_t table = id >> 16, index = id & 0xffff;
            if (table > 2 || index > 255 || CurrentMenu(MenuManager()) != GetConfig().menuDebug || logged > 5000) return;
            int64_t a[4] = {};
            if (args && !IsBadReadPtr(args, sizeof(a))) memcpy(a, args, sizeof(a));
            static std::mutex lock;
            static std::set<std::tuple<uint32_t, int64_t, int64_t, int64_t, int64_t, int64_t>> seen;
            {
                std::lock_guard guard(lock);
                if (!seen.emplace(id, a[0], a[1], a[2], a[3], result).second) return;
            }
            ++logged;
            Log(L"menu-debug: %s cmd %u(%lld, %lld, %lld, %lld) -> %lld", table == 2 ? L"g5" : table ? L"session" : L"menu", index, a[0], a[1], a[2], a[3], result);
        }

        void MenuDebug()
        {
            // Group 5 (0x1403f1d50, 146 entries; g5 #112 / #119 are the Lab's) added 2026-11-14.
            const struct { uint64_t address; int count; } tables[3] = { { 0x1403eefe0, 136 }, { 0x1403f1150, 196 }, { 0x1403f1d50, 146 } };
            for (uint32_t t = 0; t < 3; ++t)
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
            Log(L"gamepatch: menu_debug - logging menu %d's script commands", GetConfig().menuDebug);
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

        // Mods (Conceptual, mod loader step 4): the multiplayer menu's (livemain, menu 36) quick search, "Quick Match"
        // (later Quick Join), is the Mods item now: its pick opens the Mods list (HookedPushMenu) instead of the session
        // search. Its two texts in the loaded script's string pool are overwritten in place (the new ones are shorter).
        constexpr int kLiveMainMenu = 36;
        constexpr Relabel kLiveMainTexts[] = {
            { "Quick Match", "Mods" },
            { "Search for a game session with the Quick game rule.", "Turn mods on and off, and choose which wins." },
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

        // "Xbox Live" -> "Online" in every text a menu script shows (pdnet replaces Xbox Live): screen headers, the main
        // menu item and hint, pause-menu prompts, connection messages. Phrases first, so sentences still read; every
        // replacement is shorter than what it replaces, so it is written in place. Returns how many texts changed.
        int RenameXboxLive(uint8_t* script)
        {
            static const std::pair<const char*, const char*> kPhrases[] = {
                { "Connection to Xbox Live", "Connection" }, { "go to\nXbox Live?", "go online?" }, { "new Xbox Live message", "new message" },
                { "The Xbox Live service", "The service" },
                { "Use Xbox Live to connect", "Connect" }, { " to Xbox Live", " online" }, { " on Xbox Live", " online" },
                { " from Xbox Live", "" }, { " of Xbox Live", "" }, { "Xbox Live", "Online" }, { "XBOX LIVE", "ONLINE" },
            };
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                uint32_t labelsAt = code[1], strings = code[3];
                if (code[0] != 0x20) labelsAt += 4, strings += 12; // loaded: offsets relative to their own field
                if (labelsAt <= 0x20 || labelsAt > 0x40000 || strings < labelsAt || strings > 0x80000) return -1;
                char* pool = reinterpret_cast<char*>(script + strings);
                int done = 0;
                for (uint32_t* w = code + 0x20 / 4; w < code + labelsAt / 4; ++w)
                {
                    if ((*w & 0xffff) != 0x0011) continue; // push string
                    char* text = pool + (*w >> 16);
                    bool changed = false;
                    for (auto& [find, replace] : kPhrases)
                        for (char* at; (at = strstr(text, find)) != nullptr;)
                        {
                            size_t f = strlen(find), r = strlen(replace);
                            memcpy(at, replace, r);
                            memmove(at + r, at + f, strlen(at + f) + 1);
                            changed = true;
                        }
                    done += changed;
                }
                return done;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return -1;
            }
        }

        // FUN_1402aa620 prepares every script right after it is read, before it runs. The watcher (MenuTextPatches) is
        // 50 ms too late for texts used at once: the multiplayer menu types its "Xbox Live" header out on its first
        // visit. So the rename also happens here. The function starts `test rcx,rcx / jz 0x1402aa7d8`: the trampoline
        // replays the test, and the jump as an absolute one.
        using PrepareFn = void (*)(uint8_t* script);
        PrepareFn g_prepareScript = nullptr;
        void HookedPrepareScript(uint8_t* script)
        {
            if (script)
            {
                RenameXboxLive(script);
            }
            g_prepareScript(script);
        }

        void RenameAtLoad()
        {
            std::vector<uint8_t> start = { 0x48, 0x85, 0xC9, 0x0F, 0x84, 0xAF, 0x01, 0x00, 0x00 };
            std::vector<uint8_t> replay = { 0x48, 0x85, 0xC9, 0x75, 0x0E, 0xFF, 0x25, 0x00, 0x00, 0x00, 0x00 }; // test; jnz +14; jmp [rip+0]
            Put64(replay, reinterpret_cast<uint64_t>(At(0x1402aa7d8)));
            g_prepareScript = reinterpret_cast<PrepareFn>(InlineHookBytes(0x1402aa620, start, replay, reinterpret_cast<void*>(&HookedPrepareScript), L"script texts at load"));
            if (g_prepareScript) Log(L"gamepatch: Xbox Live texts are renamed Online as each script loads");
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
                    if (fresh) Log(L"gamepatch: multiplayer menu: Quick Match renamed Mods (%d of 2 texts), OptiMatch renamed Lobby Browser (%d of 1)", quick, browser);
                }
            }).detach();
        }

        // Rated / Unrated lobbies (rated_lobbies=on). The create-session menu's Rating option (No Restrictions /
        // With Restrictions: session number ratinglimit 0 / 1, originally "only players within 200 of the host")
        // becomes Unrated / Rated; the lobby search's Rating option then finds one kind or the other, and the Â±200
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
        // the script after it loads is too late for the first draw): Mods (was Quick Match / Quick Join) and Lobby Browser.
        constexpr Relabel kMenuItems[] = {
            { "Quick Match", "Mods" },
            { "OptiMatch", "Lobby Browser" },
            { "Search for a game session with the Quick game rule.", "Turn mods on and off, and choose which wins." },
            { "Select game attributes, and then search for a game session.", "Choose search options, then pick a session." },
            { "Xbox Live Options", "Online Settings" },
            { "Online Options", "Online Settings" }, // the script's text once RenameXboxLive got to it
            { "Display the Options menu.", "Your name, nametags, start-up volume and Discord posts." },
        };

        // Main menu > Multiplayer: the Online Store item (Xbox store, deprecated) is left out. It is the last item, after
        // Split Screen, so the other items keep their numbers; the shop's own $STORE item is not touched.
        // The multiplayer menu's Trade (after Create Match; the trade screens say "Trade" too) reads Theater for now.
        struct ItemAfter { const char* previous; const char* find; const char* replace; }; // replace nullptr: left out
        constexpr ItemAfter kItemsAfter[] = {
            { "Split Screen", "$STORE", nullptr },
            { "Battle another player using split screen.", "$STORE_HINT", nullptr },
            { "Create Match", "Trade", "Theater" },
            { "Create a new game session.", "Trade a skill.", "Watch films of your recent online matches." }, // the Theatre
        };
        char g_lastItem[80]; // the text of the item added before (the menu's own text, before any renaming)

        bool SettingsItems(const char* added); // the in-game Settings rows (below)
        bool SoundItems(const char* added, int64_t* args, int64_t& result); // the Options > Sound rows (below)
        bool LogItems(const char* added, int64_t* args, int64_t& result);   // Options > Send Log (below)
        bool CaseItems(const char* added, int64_t* args, int64_t& result);  // Arsenal Lab > Buy / Sell Arsenal Case (below)

        int64_t HookedAddItem(int64_t* args)
        {
            if (args)
            {
                char text[80];
                if (CopyString(reinterpret_cast<const char*>(args[0]), text, sizeof(text)))
                {
                    // Only the scoreboard menu adds $LEADERBOARD_ items.
                    if (GetConfig().scoreboard && strncmp(text, "$LEADERBOARD_", 13) == 0 && atoi(text + 13) % 3 != 0) return 0; // Friends / Favorites
                    if (g_testPicking == kPickingOpponentArsenal && strcmp(text, "Select an arsenal.") == 0) // livearsel, the AI's pick in Test Arsenal
                    {
                        int64_t renamed[8];
                        memcpy(renamed, args, sizeof(renamed));
                        renamed[0] = reinterpret_cast<int64_t>("Choose the arsenal the AI OPPONENT fights with.");
                        return g_addItem(renamed);
                    }
                    if ((g_testPicking == kPickingCharacter || g_testPicking == kPickingOpponentCharacter) && strcmp(text, "Select a character.") == 0) // livecharasel, twice in Test Arsenal
                    {
                        int64_t renamed[8];
                        memcpy(renamed, args, sizeof(renamed));
                        bool opponent = g_testPicking == kPickingOpponentCharacter;
                        renamed[0] = reinterpret_cast<int64_t>(opponent ? "Choose the character the AI OPPONENT plays as." : "Choose the character YOU play as.");
                        SetMenuHeading(opponent ? "AI OPPONENT'S CHARACTER" : "YOUR CHARACTER");
                        return g_addItem(renamed);
                    }
                    char previous[80];
                    memcpy(previous, g_lastItem, sizeof(previous));
                    memcpy(g_lastItem, text, sizeof(g_lastItem));
                    for (auto& item : kItemsAfter)
                        if (strcmp(text, item.find) == 0 && strcmp(previous, item.previous) == 0)
                        {
                            if (!item.replace) return 0;
                            int64_t renamed[8];
                            memcpy(renamed, args, sizeof(renamed));
                            renamed[0] = reinterpret_cast<int64_t>(item.replace);
                            return g_addItem(renamed);
                        }
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
                    if (int64_t result = 0; LogItems(text, args, result)) return result;   // followed by Send Log
                    if (int64_t result = 0; CaseItems(text, args, result)) return result;  // followed by Buy / Sell Arsenal Case
                    return g_addItem(args);
                }
            }
            return g_addItem(args);
        }
        void SettingsScriptPatch(int menu, uint8_t* script, bool fresh); // the in-game Settings menu (below)
        void SettingsTick();

        // Every menu script as it loads (menu table, 87 entries): the Rated texts, the scoreboard list, Xbox Live -> Online.
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
                        if (m == kCaseBuyMenu && MultiplayerProfile())
                        {
                            // The list row is drawn with the school number as its first value: CaseRowText changes it as drawn.
                            static constexpr Relabel kCaseTexts[] = {
                                { "Do you want to proceed with the\npurchase of an Arsenal Case with\na %d-School Restriction?",
                                  "Do you want to proceed with the\npurchase of an Arsenal Case?" },
                                { "Purchased: Arsenal Case with\n%d-School Restriction", "Purchased: Arsenal Case" },
                            };
                            RelabelScript(script, kCaseTexts, std::size(kCaseTexts));
                        }
                        if (int renamed = RenameXboxLive(script); fresh && renamed > 0) Log(L"gamepatch: menu %d: %d Xbox Live text(s) now say Online", m, renamed);
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
            if (AiBattle())
            {
                Log(L"gamepatch: rating change %lld not applied (battle against an AI)", change);
                return g_applyRating(a, 0, c, d, e, f);
            }
            if (rated == 1 && mods::InUse()) // gameplay mods: Unrated (the lobby says so too, PD_RATED 0)
            {
                Log(L"gamepatch: rating change %lld not applied (gameplay mods: Unrated)", change);
                change = 0;
                rated = 2;
            }
            if (rated != 1 && rated != 2)
            {
                Log(L"gamepatch: rating change %lld not applied (%s match)", change, rated == 0 ? L"Unrated" : L"unknown -> Unrated");
                change = 0;
            }
            else if (rated == 1) Log(L"gamepatch: rating change %lld (Rated match)", change);
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
            // One case each press (the user's choice), up to the game's 16. In the multiplayer profile its number then follows
            // what's put in it (dynamic cases).
            int cases = *reinterpret_cast<int32_t*>(save + 0x3a48);
            bool gaveCase = cases < 16;
            if (gaveCase) reinterpret_cast<GiveCaseFn>(At(0x14015c0e0))(mode, kCaseSchools);
            Log(L"gamepatch: shop Max Credits & Skills (%s profile): %d skill(s) topped up to %d, credits %d -> %d, Arsenal Case %s",
                mode == 2 ? L"multiplayer" : L"story", skills, kSkillTarget, before, credits, gaveCase ? L"given" : L"not given (16 cases)");
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
                const char hint[] = "Up to 5,000,000 credits, 80 of each skill, and an Arsenal Case.";
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

        // Theatre: the multiplayer menu's Theater item (was Trade, livetrade 40) opens this same
        // screen with the saved films as its rows (film::List), newest first, and each film's details as the row's hint.
        // A on a film picks it; playback isn't built yet, so that is only logged for now. The screen fits 10 rows: all the
        // films (up to 100 kept) are paged through 10 at a time with LB / RB (or Page Up / Page Down, Q / E); the rows'
        // texts are buffers the window keeps reading, so a page change only rewrites them.
        constexpr int kTradeMenu = 40, kTheatreRows = 10, kTheatreFilms = 100;
        std::vector<film::Info> g_films;
        char g_filmRow[kTheatreRows][112];
        char g_filmHint[kTheatreRows][160];
        int g_filmRows = 0, g_filmPage = 0;
        // \a5# is the Y icon (the character select's "Y Advanced"), \a2# A, \a3# B. The bar has no LB / RB icons, so the
        // page keys are plain text.
        const char* kTheatreBar = "\a8#Select  LB / RB Page \a2#Watch \a580r\a5#Delete \a3#Back";

        void StartFilmBattle(const film::Info& f); // (below)
        int FilmPages() { return g_films.empty() ? 1 : static_cast<int>((g_films.size() + kTheatreRows - 1) / kTheatreRows); }

        void FillFilmPage()
        {
            if (g_films.empty())
            {
                sprintf_s(g_filmRow[0], "\a160wNo films yet\f\a1#\a230c\f\a0#");
                strcpy_s(g_filmHint[0], "Every match is saved as a film. Play one, then come back.");
                return;
            }
            char page[48] = "";
            if (FilmPages() > 1) sprintf_s(page, "  (page %d of %d: LB / RB)", g_filmPage + 1, FilmPages());
            for (int r = 0; r < g_filmRows; ++r)
            {
                size_t i = static_cast<size_t>(g_filmPage) * kTheatreRows + r;
                if (i >= g_films.size())
                {
                    g_filmRow[r][0] = 0;
                    strcpy_s(g_filmHint[r], page);
                    continue;
                }
                const film::Info& f = g_films[i];
                const char* type = film::MatchType(f);
                char length[24], label[48], lobby[64] = "";
                sprintf_s(length, "%d:%02d", static_cast<int>(f.minutes), static_cast<int>(f.minutes * 60) % 60);
                // Numbered films (Conceptual 38 on): "Lobby 3  Match 2"; older ones: their date.
                if (f.lobbySession) sprintf_s(label, "Lobby %u  Match %u", f.lobbySession, f.matchInLobby);
                else strcpy_s(label, f.when.empty() ? "Film" : f.when.c_str());
                if (f.lobbySession) sprintf_s(lobby, " %s, lobby %.8s.", f.when.c_str(), f.lobby.empty() ? "offline" : f.lobby.c_str() + (f.lobby.size() > 10 ? 2 : 0));
                _snprintf_s(g_filmRow[r], _TRUNCATE, "\a160w%s\f\a1#\a230c%s %s\f\a0#", label, type, length);
                _snprintf_s(g_filmHint[r], _TRUNCATE, "%s on stage %d: %s.%s Y / Delete: delete it.%s", type, f.stage, f.players.empty() ? "players not recorded" : f.players.c_str(), lobby, page);
            }
        }

        // Deleting a film (the user, 2026-11-21): Y (or the Delete key) on a film's row asks on that row ("Delete this
        // film?"), Y / Delete again within 5 s deletes it (film::Delete); moving to another row, or waiting, keeps it.
        int g_deleteRow = -1;
        ULONGLONG g_deleteAskedAt = 0;
        void RefreshFilmRows()
        {
            g_films = film::List(kTheatreFilms);
            g_filmPage = 0;
            g_filmRows = g_films.empty() ? 1 : (std::min)(kTheatreRows, static_cast<int>(g_films.size()));
            FillFilmPage();
        }

        void TurnFilmPage(int step)
        {
            int pages = FilmPages();
            if (pages <= 1) return;
            g_filmPage = (g_filmPage + step + pages) % pages;
            FillFilmPage();
            Log(L"theatre: page %d of %d", g_filmPage + 1, pages);
        }
        // Mods (Conceptual, mod loader step 4): the multiplayer menu's Mods item (was Quick Join) opens this same screen with
        // the mods as its rows, from LocalState\pdnet\mods-state.txt: what pdlaunch put in PDPL's mods folder at this game
        // start (launcher\modsync.cpp), in priority order (the top one wins when two change the same file). A turns the
        // highlighted mod on or off; X picks it up, up / down then carry it, X puts it down; Y sorts (load_after /
        // load_before first, then cosmetic mods above models, maps, skills and the rest); LB / RB turn pages. Leaving saves
        // mods_order and mods_enabled to settings.ini, and pdlaunch applies them at the next game start (PDPL 1.2.2 can't be
        // told to change its mods while the game runs). The player's own PDPL archives (RoamingState\mods) stay below the
        // others in name order, as PDPL mounts them by name.
        constexpr int kModRows = 10;
        struct ModRow
        {
            std::string id, source, name, version, author, online, problem, description, pluginFiles, warning;
            std::vector<std::string> touches, after, before;
            bool on = true, plugins = false;
            int files = 0;
            bool Movable() const { return source != "pdpl"; }
        };
        std::vector<ModRow> g_mods;
        std::vector<std::tuple<std::string, std::string, int>> g_modOverlaps; // two mods, how many game files both have
        int g_looseFiles = 0;
        char g_modRow[kModRows][112];
        char g_modHint[kModRows][300];
        int g_modRows = 0, g_modPage = 0, g_movingMod = -1;
        std::atomic<bool> g_modsDirty{ false };
        // \a2# A, \a3# B, \a5# Y (the lobby's "\a5#Invite"), \a4# X (the character select's X "Toggle Monsters").
        const char* kModsBar = "\a8#Select  LB / RB Page \a2#On / Off \a580r\a4#Move \a5#Auto Sort \a3#Back";

        std::vector<std::string> SplitFields(const std::string& s, char sep) // empty fields kept
        {
            std::vector<std::string> out(1);
            for (char c : s)
                if (c == sep) out.emplace_back();
                else out.back() += c;
            return out;
        }

        std::vector<std::string> SplitList(const std::string& s)
        {
            std::vector<std::string> out;
            for (auto& x : SplitFields(s, ','))
                if (!x.empty()) out.push_back(x);
            return out;
        }

        void LoadModList()
        {
            g_mods.clear();
            g_modOverlaps.clear();
            g_looseFiles = 0;
            std::ifstream in(LocalStatePath() + L"\\pdnet\\mods-state.txt", std::ios::binary);
            std::string line;
            while (std::getline(in, line))
            {
                if (!line.empty() && line.back() == '\r') line.pop_back();
                auto f = SplitFields(line, '\t');
                if (f[0] == "loose" && f.size() > 1) g_looseFiles = atoi(f[1].c_str());
                else if (f[0] == "conflict" && f.size() > 3) g_modOverlaps.emplace_back(f[1], f[2], atoi(f[3].c_str()));
                else if (f[0] == "mod" && f.size() > 12)
                {
                    ModRow m;
                    m.id = f[1];
                    m.on = f[2] == "on";
                    m.source = f[3];
                    m.name = f[4];
                    m.version = f[5];
                    m.author = f[6];
                    m.online = f[7];
                    m.touches = SplitList(f[8]);
                    m.plugins = f[9] == "1";
                    m.files = atoi(f[10].c_str());
                    m.problem = f[11];
                    if (f.size() > 13) m.after = SplitList(f[13]);
                    if (f.size() > 14) m.before = SplitList(f[14]);
                    if (f.size() > 15) m.description = f[15];
                    if (f.size() > 18) m.pluginFiles = f[18]; // plugin DLLs (step 8), ';'-separated
                    if (f.size() > 19) m.warning = f[19];
                    g_mods.push_back(std::move(m));
                }
            }
            Log(L"mods: list: %zu mod(s) from mods-state.txt", g_mods.size());
        }

        int ModPages() { return g_mods.empty() ? 1 : static_cast<int>((g_mods.size() + kModRows - 1) / kModRows); }
        int ModIndex(const std::string& id)
        {
            for (size_t i = 0; i < g_mods.size(); ++i)
                if (g_mods[i].id == id) return static_cast<int>(i);
            return -1;
        }
        int LastMovableMod()
        {
            int last = -1;
            for (size_t i = 0; i < g_mods.size(); ++i)
                if (g_mods[i].Movable()) last = static_cast<int>(i);
            return last;
        }

        // "Wins over X for 12 files" / "X wins for 3 files": with both on, the one higher in the list wins.
        std::string ModOverlapText(int i)
        {
            std::string wins, loses;
            for (const auto& [a, b, n] : g_modOverlaps)
            {
                const std::string* other = a == g_mods[i].id ? &b : b == g_mods[i].id ? &a : nullptr;
                int j = other ? ModIndex(*other) : -1;
                if (j < 0 || !g_mods[i].on || !g_mods[j].on) continue;
                char part[96];
                sprintf_s(part, "%s%s (%d file%s)", (j > i ? wins : loses).empty() ? "" : ", ", g_mods[j].name.c_str(), n, n == 1 ? "" : "s");
                (j > i ? wins : loses) += part;
            }
            std::string text;
            if (!wins.empty()) text += " Wins over " + wins + ".";
            if (!loses.empty()) text += " Loses to " + loses + ".";
            return text;
        }

        void FillModPage()
        {
            if (g_mods.empty())
            {
                strcpy_s(g_modRow[0], "\a160wNo mods yet\f\a1#\a230c\f\a0#");
                strcpy_s(g_modHint[0], "Put a .zip or .7z in LocalState\\pdnet\\mods (or PDPL's RoamingState\\mods), then start the game again.");
                return;
            }
            char page[48] = "";
            if (ModPages() > 1) sprintf_s(page, " (page %d of %d: LB / RB)", g_modPage + 1, ModPages());
            for (int r = 0; r < g_modRows; ++r)
            {
                size_t i = static_cast<size_t>(g_modPage) * kModRows + r;
                if (i >= g_mods.size())
                {
                    g_modRow[r][0] = 0;
                    strcpy_s(g_modHint[r], page);
                    continue;
                }
                const ModRow& m = g_mods[i];
                bool notDownloaded = m.source == "catalog" && m.problem.rfind("not downloaded", 0) == 0;
                const char* kind = notDownloaded ? "Download" : m.source == "pdpl" ? "PDPL" : m.online == "local" ? "Only you" : "Online";
                _snprintf_s(g_modRow[r], _TRUNCATE, "\a160w%s[%s] %s\f\a1#\a230c%s %s\f\a0#", static_cast<int>(i) == g_movingMod ? "> " : "",
                            m.on ? "x" : "  ", m.name.c_str(), m.version.c_str(), kind);
                std::string hint = notDownloaded ? "From the mod catalog: " + m.name + "." : m.description.empty() ? m.name : m.description;
                if (!m.author.empty()) hint += " By " + m.author + ".";
                hint += ModOverlapText(static_cast<int>(i));
                if (!m.pluginFiles.empty()) hint += " Contains code (" + m.pluginFiles + "): it runs inside the game.";
                if (!m.warning.empty()) hint += " (" + m.warning + ")";
                if (notDownloaded) hint += m.on ? " Downloads at the next game start." : " Turn it on and it downloads at the next game start.";
                if (notDownloaded && m.problem != "not downloaded") hint += " (" + m.problem.substr(16) + ")"; // after "not downloaded: "
                else if (!notDownloaded && !m.problem.empty()) hint += " (" + m.problem + ")";
                if (!m.Movable()) hint += " PDPL folder: ordered by file name.";
                if (static_cast<int>(i) == g_movingMod) hint += " Up / down to move, X to put it down.";
                if (g_modsDirty) hint += "\nChanges apply at the next game start.";
                hint += page;
                strcpy_s(g_modHint[r], hint.substr(0, sizeof(g_modHint[r]) - 1).c_str());
            }
        }

        void RefreshModRows()
        {
            LoadModList();
            g_modPage = 0;
            g_movingMod = -1;
            g_modsDirty = false;
            g_modRows = g_mods.empty() ? 1 : (std::min)(kModRows, static_cast<int>(g_mods.size()));
            FillModPage();
        }

        void ToggleMod(size_t i)
        {
            if (i >= g_mods.size()) return;
            g_mods[i].on = !g_mods[i].on;
            g_modsDirty = true;
            Log(L"mods menu: %S %s", g_mods[i].id.c_str(), g_mods[i].on ? L"on" : L"off");
            FillModPage();
        }

        // The mod being carried goes to `target` (its place among the movable mods; the player's PDPL archives stay put).
        void MoveModTo(size_t target)
        {
            int last = LastMovableMod();
            if (g_movingMod < 0 || last < 0) return;
            int to = (std::min)(static_cast<int>(target), last);
            if (to == g_movingMod) return;
            ModRow m = std::move(g_mods[g_movingMod]);
            g_mods.erase(g_mods.begin() + g_movingMod);
            g_mods.insert(g_mods.begin() + to, std::move(m));
            g_movingMod = to;
            g_modsDirty = true;
            FillModPage();
        }

        void TurnModPage(int step, int row)
        {
            int pages = ModPages();
            if (pages <= 1) return;
            g_modPage = (g_modPage + step + pages) % pages;
            if (g_movingMod >= 0 && row >= 0) MoveModTo(static_cast<size_t>(g_modPage) * kModRows + row); // carried along
            FillModPage();
        }

        // Auto sort: cosmetic mods (only looks or sound, or "online": "local") at the top, then models / animation, maps,
        // skills / text, and plugins / anything else at the bottom; load_after / load_before always kept ("load_after": X =
        // my files win over X's, so I go above X). Ties by name. A loop in load_after / load_before keeps its current order.
        int ModTier(const ModRow& m)
        {
            auto has = [&](const char* k) { return std::find(m.touches.begin(), m.touches.end(), k) != m.touches.end(); };
            bool cosmetic = !m.touches.empty() && std::all_of(m.touches.begin(), m.touches.end(), [](const std::string& t) { return t == "ui" || t == "audio" || t == "shaders"; });
            if (cosmetic || m.online == "local") return 0;
            if (has("plugins") || has("other")) return 4;
            if (has("skills") || has("text")) return 3;
            if (has("maps")) return 2;
            return 1;
        }

        void AutoSortMods()
        {
            std::vector<ModRow> movable, fixed;
            for (auto& m : g_mods) (m.Movable() ? movable : fixed).push_back(m);
            size_t n = movable.size();
            std::vector<std::vector<size_t>> below(n); // below[a]: the mods a must be above
            std::vector<int> above(n, 0);              // how many must be above each
            auto index = [&](const std::string& id) { for (size_t i = 0; i < n; ++i) if (movable[i].id == id) return static_cast<int>(i); return -1; };
            auto edge = [&](int a, int b) { if (a >= 0 && b >= 0 && a != b) { below[a].push_back(static_cast<size_t>(b)); ++above[b]; } };
            for (size_t a = 0; a < n; ++a)
            {
                for (auto& id : movable[a].after) edge(static_cast<int>(a), index(id));
                for (auto& id : movable[a].before) edge(index(id), static_cast<int>(a));
            }
            std::vector<bool> placed(n, false);
            std::vector<ModRow> sorted;
            auto lower = [](std::string s) { for (char& c : s) c = static_cast<char>(tolower(static_cast<unsigned char>(c))); return s; };
            for (size_t done = 0; done < n; ++done)
            {
                int pick = -1;
                for (size_t i = 0; i < n; ++i)
                {
                    if (placed[i] || above[i] > 0) continue;
                    if (pick < 0 || std::make_pair(ModTier(movable[i]), lower(movable[i].name)) < std::make_pair(ModTier(movable[pick]), lower(movable[pick].name))) pick = static_cast<int>(i);
                }
                if (pick < 0) // a loop: the first unplaced one in the current order goes next
                {
                    for (size_t i = 0; i < n && pick < 0; ++i)
                        if (!placed[i]) pick = static_cast<int>(i);
                    movable[pick].problem = "load order loop";
                    Log(L"mods menu: auto sort: load_after / load_before loop at %S", movable[pick].id.c_str());
                }
                placed[pick] = true;
                for (size_t b : below[pick]) --above[b];
                sorted.push_back(movable[pick]);
            }
            sorted.insert(sorted.end(), fixed.begin(), fixed.end());
            bool changed = false;
            for (size_t i = 0; i < sorted.size(); ++i) changed = changed || sorted[i].id != g_mods[i].id;
            g_mods = std::move(sorted);
            g_movingMod = -1;
            if (changed) g_modsDirty = true;
            std::string order;
            for (auto& m : g_mods) order += (order.empty() ? "" : ", ") + m.id;
            Log(L"mods menu: auto sort%s: %S", changed ? L"" : L" (no change)", order.c_str());
            FillModPage();
        }

        // Leaving the list (back, or any other way out): changes go to settings.ini for the next game start.
        void LeaveModList()
        {
            g_movingMod = -1;
            if (!g_modsDirty.exchange(false)) return;
            std::string order, enabled;
            for (auto& m : g_mods)
            {
                order += (order.empty() ? "" : ",") + m.id;
                if (m.on) enabled += (enabled.empty() ? "" : ",") + m.id;
            }
            SaveSettings({ { "mods_order", order }, { "mods_enabled", enabled } });
            Log(L"mods menu: saved for the next game start - order %S; on: %S", order.c_str(), enabled.empty() ? "none" : enabled.c_str());
        }

        struct Colour { const char* name; uint32_t rgb; };
        constexpr Colour kColours[] = { { "Yellow", 0xFFE600 }, { "Red", 0xFF5A5A }, { "Blue", 0x4FB4FF }, { "Green", 0x5AFF6E },
                                        { "Purple", 0xB46EFF }, { "Orange", 0xFFA03C }, { "Cyan", 0x3CF0FF }, { "Pink", 0xFF78C8 },
                                        { "White", 0xFFFFFF } };
        enum SettingRow { kName, kVoiceMode, kPartnerAim, kBodyTags, kDeadColour, kAllyColour, kEnemyColour, kSplashVolume,
                          kDiscordLobby, kDiscordPlayers, kSettingRows };
        char g_rowText[kSettingRows][80];
        char g_optionText[kSettingRows][96]; // the rows as the Options screen shows them (RefreshSettingRows)
        constexpr int kFirstOptionRow = 3;   // after Cross Play (1) and Voice Chat (2)
        const char* kOptionLabels[kSettingRows] = { "Name", "Voice Mode", "Partner Aim", "Body Tags", "Dead Colour", "Ally Colour", "Foe Colour",
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
            sprintf_s(g_rowText[kPartnerAim], "Partner aim: %s", c.partnerClassic ? "Classic" : "Modern");
            sprintf_s(g_rowText[kBodyTags], "Nametags on bodies: %s", onOff(c.nametagsOnBodies));
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
            case kVoiceMode: return "Modern: Mic mutes only you. Classic: Off mutes everyone.";
            case kPartnerAim: return "Modern: RB locks onto partner. Classic: RB adds them to switch target.";
            case kBodyTags: return "Defeated players keep their nametag (Tag Team; the host's applies).";
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
                EditableConfig().displayName = name.substr(0, 16);
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
                                else if (ch >= 32 && ch < 0xFFFF && g_typedName.size() < 16) g_typedName += static_cast<wchar_t>(ch);
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
            case kPartnerAim: c.partnerClassic = !c.partnerClassic; break;
            case kBodyTags: c.nametagsOnBodies = !c.nametagsOnBodies; UseOwnSettings(); break;
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

        // B during the start-up logos. playdatasel's wait routine (0x7e8) counts its frames in a local: i += 1, then, when its
        // second argument is set, i = g10 #17 (0). PatchLogoWait has it call this instead (arguments: that argument, i), so
        // B can end the wait; the logo then fades out as usual.
        constexpr int kStartMenu = 7, kLogoReturnBig = 0x7fffffff;
        int64_t LogoWait(int32_t* args)
        {
            int64_t input[2] = { 0, 0 };
            bool cancel = (reinterpret_cast<InputFn>(At(0x1402168a0))(input) & 0x2020) != 0;
            int64_t counted = args[2];
            if (args[0])
            {
                int64_t none[8] = {};
                counted = (*reinterpret_cast<InputFn*>(At(0x1403ddde0 + 17 * 8)))(none);
            }
            static std::atomic<bool> told{ false };
            if (cancel && !told.exchange(true)) Log(L"gamepatch: B: start-up logo skipped");
            return cancel ? kLogoReturnBig : counted;
        }

        int64_t HookedOptionChanged(int32_t* args)
        {
            if (args && CurrentMenu(MenuManager()) == kStartMenu) return LogoWait(args);
            if (!InSettings() || !args) return g_optionChanged ? g_optionChanged(args) : 0;
            int64_t input[2] = { 0, 0 };
            int64_t buttons = reinterpret_cast<InputFn>(At(0x1402168a0))(input); // g1 #37 (0): the buttons, as the script reads them
            int row = args[0] - kFirstOptionRow;
            if (g_theatre)
            {
                static ULONGLONG last = 0;
                size_t pick = static_cast<size_t>(g_filmPage) * kTheatreRows + row;
                if ((buttons & 0x1010) && row >= 0 && row < kTheatreRows && pick < g_films.size() && GetTickCount64() - last > 300)
                {
                    last = GetTickCount64();
                    int64_t sound[2] = { 0, 0 };
                    reinterpret_cast<InputFn>(At(0x140261750))(sound); // the menu's select sound
                    Log(L"theatre: film %s chosen (%S, %S) - playback isn't built yet", g_films[pick].file.c_str(), g_films[pick].when.c_str(), g_films[pick].players.c_str());
                }
                return 0;
            }
            if (g_modsList) return 0; // left / right: nothing (A, X and Y are read in HookedButtons)
            if (row >= 0 && row < kSettingRows)
            {
                int64_t sound[2] = { 37, 0 };
                reinterpret_cast<InputFn>(At(0x140261750))(sound); // g1 #43 (37): the change sound the game's own rows play
                ChangeSetting(row, (buttons & 0x00040004) ? -1 : 1);
            }
            return 0;
        }

        constexpr Relabel kSettingsTexts[] = {
            { "XBOX LIVE OPTIONS", "ONLINE SETTINGS" },
            { "ONLINE OPTIONS", "ONLINE SETTINGS" }, // once RenameXboxLive got to it (it fits: the text was "XBOX LIVE OPTIONS")
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
            SaveSettings({ { "display_name", winrt::to_string(c.displayName) }, { "voice_mode", c.voiceClassic ? "classic" : "mic" }, { "partner_aim", c.partnerClassic ? "classic" : "modern" },
                           { "nametags_on_bodies", onOff(c.nametagsOnBodies) },
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
            c.displayName = s.displayName; c.voiceClassic = s.voiceClassic; c.partnerClassic = s.partnerClassic; c.nametagsOnBodies = s.nametagsOnBodies;
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
            if (!InSettings() && g_modsDirty && GetTickCount64() - g_settingsOpenedAt > 1000) LeaveModList(); // the Mods list left some other way than back
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
        // the first line of kit-version.txt next to pdnet ("PhantomDust-pdnet Experimental 29" -> "Rebirth Experimental 29"),
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
                // No kit-version.txt: build-linux.sh makes only the DLL, which goes in the game's folder, so it names itself.
#ifndef PDNET_BUILD_LABEL
#define PDNET_BUILD_LABEL "development build"
#endif
                return "Rebirth " + (first.empty() ? std::string(PDNET_BUILD_LABEL) : first);
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

        int64_t CharacterSelectButtons(int64_t buttons); // (below)
        void DrawCharselPopup(int64_t* args);

        int64_t HookedDrawWindow(int64_t* args)
        {
            int64_t result = g_drawWindow(args);
            if (CurrentMenu(MenuManager()) == kCharacterMenu) { DrawCharselPopup(args); return result; }
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

        int TheatreCursorRow()
        {
            int64_t manager = MenuManager();
            int window = manager ? *reinterpret_cast<int16_t*>(manager + 0x10402) : -1;
            return window >= 0 && window < 16 ? WindowAt(manager, window)[0x12 / 2] : -1;
        }

        void TheatreDelete()
        {
            static bool held = false;
            uint8_t keys[256];
            bool press = AnyPadHolds(0x20) || (KeysHeld(keys) && keys[VK_DELETE]); // Y, Delete
            bool fresh = press && !held;
            held = press;
            int row = TheatreCursorRow();
            if (g_deleteRow >= 0 && (row != g_deleteRow || GetTickCount64() - g_deleteAskedAt > 5000))
            {
                g_deleteRow = -1;
                FillFilmPage(); // the row's own text back
            }
            if (!fresh) return;
            size_t pick = static_cast<size_t>(g_filmPage) * kTheatreRows + row;
            if (row < 0 || row >= kTheatreRows || pick >= g_films.size()) return;
            int64_t sound[2] = { 37, 0 };
            if (g_deleteRow != row)
            {
                g_deleteRow = row;
                g_deleteAskedAt = GetTickCount64();
                strcpy_s(g_filmRow[row], "\a160wDelete this film?\f\a1#\a230cY again\f\a0#");
                strcpy_s(g_filmHint[row], "Press Y (or Delete) again to delete this film for good. Move to another row to keep it.");
                reinterpret_cast<InputFn>(At(0x140261750))(sound);
                return;
            }
            g_deleteRow = -1;
            std::wstring file = g_films[pick].file;
            if (film::Delete(file))
            {
                g_films.erase(g_films.begin() + static_cast<std::ptrdiff_t>(pick));
                if (g_filmPage >= FilmPages()) g_filmPage = FilmPages() - 1;
                sound[0] = 0;
            }
            else sound[0] = 1;
            reinterpret_cast<InputFn>(At(0x140261750))(sound);
            for (int r = 0; r < kTheatreRows; ++r) { g_filmRow[r][0] = 0; g_filmHint[r][0] = 0; }
            FillFilmPage();
        }

        int64_t HookedButtons(int64_t* args)
        {
            int64_t buttons = g_buttons(args);
            if ((!args || args[0] == 0) && CurrentMenu(MenuManager()) == kCharacterMenu) return CharacterSelectButtons(buttons);
            if (InSettings() && g_theatre && (!args || args[0] == 0))
            {
                static bool held[2] = {};
                uint8_t keys[256];
                bool haveKeys = KeysHeld(keys);
                bool next = AnyPadHolds(0x800) || (haveKeys && (keys[VK_NEXT] || keys['E'])); // RB, Page Down, E
                bool previous = AnyPadHolds(0x400) || (haveKeys && (keys[VK_PRIOR] || keys['Q'])); // LB, Page Up, Q
                if (next && !held[0] || previous && !held[1])
                {
                    int64_t sound[2] = { 37, 0 };
                    reinterpret_cast<InputFn>(At(0x140261750))(sound);
                    TurnFilmPage(next && !held[0] ? 1 : -1);
                }
                held[0] = next;
                held[1] = previous;
                TheatreDelete();
                // A: the film on the highlighted row. The window's cursor is its +0x12 (FUN_14024b000 moves it), the screen's
                // window the manager's +0x10402; pdnet's rows are the window's only rows, so the cursor is the row. A is kept
                // from the script (it would run the row's old action).
                static ULONGLONG lastPick = 0;
                if ((buttons & 0x1010) && GetTickCount64() - lastPick > 300)
                {
                    lastPick = GetTickCount64();
                    int64_t manager = MenuManager();
                    int window = manager ? *reinterpret_cast<int16_t*>(manager + 0x10402) : -1;
                    int row = window >= 0 && window < 16 ? WindowAt(manager, window)[0x12 / 2] : -1;
                    size_t pick = static_cast<size_t>(g_filmPage) * kTheatreRows + row;
                    if (row >= 0 && row < kTheatreRows && pick < g_films.size())
                    {
                        int64_t sound[2] = { 0, 0 };
                        reinterpret_cast<InputFn>(At(0x140261750))(sound); // the menu's select sound
                        Log(L"theatre: film %s chosen (%S, %S)", g_films[pick].file.c_str(), g_films[pick].when.c_str(), g_films[pick].players.c_str());
                        StartFilmBattle(g_films[pick]);
                    }
                    else Log(L"theatre: A on row %d (window %d): no film there", row, window);
                }
                return buttons & ~0x1010ll;
            }
            if (InSettings() && g_modsList && (!args || args[0] == 0))
            {
                // Each button acts once per press: the script reads the buttons several times a frame.
                static bool held[5] = {}; // RB, LB, X, Y, A
                uint8_t keys[256];
                bool haveKeys = KeysHeld(keys);
                bool now[5] = {
                    AnyPadHolds(0x800) || (haveKeys && (keys[VK_NEXT] || keys['E'])),  // RB, Page Down, E: next page
                    AnyPadHolds(0x400) || (haveKeys && (keys[VK_PRIOR] || keys['Q'])), // LB, Page Up, Q: previous page
                    AnyPadHolds(0x10) || (haveKeys && keys['X']),                      // X: pick up / put down
                    AnyPadHolds(0x20) || (haveKeys && keys['Y']),                      // Y: auto sort
                    (buttons & 0x1010) != 0,                                           // A: on / off
                };
                bool pressed[5];
                for (int i = 0; i < 5; ++i) { pressed[i] = now[i] && !held[i]; held[i] = now[i]; }
                int64_t manager = MenuManager();
                int window = manager ? *reinterpret_cast<int16_t*>(manager + 0x10402) : -1;
                int row = window >= 0 && window < 16 ? WindowAt(manager, window)[0x12 / 2] : -1; // the cursor (as in the Theatre)
                size_t at = static_cast<size_t>(g_modPage) * kModRows + (row >= 0 ? row : 0);
                auto sound = [](int64_t which) { int64_t s[2] = { which, 0 }; reinterpret_cast<InputFn>(At(0x140261750))(s); };
                if (g_movingMod >= 0 && row >= 0 && row < kModRows && static_cast<int>(at) != g_movingMod && at < g_mods.size()) MoveModTo(at); // carried with the cursor
                if (pressed[0] || pressed[1])
                {
                    sound(37);
                    TurnModPage(pressed[0] ? 1 : -1, row);
                }
                if (pressed[4] && row >= 0 && at < g_mods.size())
                {
                    sound(37);
                    ToggleMod(at);
                }
                if (pressed[2] && row >= 0 && at < g_mods.size())
                {
                    if (g_movingMod >= 0)
                    {
                        Log(L"mods menu: %S put down at %d", g_mods[g_movingMod].id.c_str(), g_movingMod + 1);
                        g_movingMod = -1;
                        sound(0);
                    }
                    else if (g_mods[at].Movable())
                    {
                        g_movingMod = static_cast<int>(at);
                        sound(0);
                    }
                    else sound(1); // the player's PDPL archives go by file name
                    FillModPage();
                }
                if (pressed[3] && !g_mods.empty())
                {
                    sound(37);
                    AutoSortMods();
                }
                return buttons & ~0x1010ll; // A is the list's, not the script's row action
            }
            if (!InSettings() || (args && args[0] != 0) || g_theatre || g_modsList) return buttons;
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
        bool CharacterSelectBar(const char* text, char* out, size_t size); // (below)
        int64_t HookedButtonBar(int64_t* args)
        {
            const char* kSave = g_theatre ? kTheatreBar : g_modsList ? kModsBar : g_confirmingSave ? kConfirmBar : kNormalBar;
            char text[64];
            if (args && args[0] == 1 && CopyString(reinterpret_cast<const char*>(args[1]), text, sizeof(text)) && strcmp(text, "LOADING") == 0 &&
                CurrentMenu(MenuManager()) == kLobbyMenu && !g_matchLoadingSince)
                g_matchLoadingSince = GetTickCount64(); // the lobby's loading screen, on the way into a match
            if (args && (InSettings() || GetTickCount64() - g_settingsOpenedAt < 500) && CopyString(reinterpret_cast<const char*>(args[1]), text, sizeof(text)) &&
                (strcmp(text, "\a7#Change \a580r\a3#Back") == 0 || strcmp(text, "\a8#Select \a7#Change \a580r\a3#Back") == 0))
            {
                int64_t changed[8];
                memcpy(changed, args, sizeof(changed));
                changed[1] = reinterpret_cast<int64_t>(kSave);
                return g_buttonBar(changed);
            }
            static char charselBar[160];
            if (args && CopyString(reinterpret_cast<const char*>(args[1]), charselBar, sizeof(charselBar)))
            {
                static char withY[200];
                if (CharacterSelectBar(charselBar, withY, sizeof(withY)))
                {
                    int64_t changed[8];
                    memcpy(changed, args, sizeof(changed));
                    changed[1] = reinterpret_cast<int64_t>(withY);
                    return g_buttonBar(changed);
                }
            }
            return g_buttonBar(args);
        }

        bool SettingsItems(const char* added)
        {
            bool rows = strcmp(added, "\a160wVoice Chat\f\a1#\a230c%s\f\a0#") == 0, hints = strcmp(added, "Set your voice chat status.") == 0;
            if (!rows && !hints) return false;
            if (g_modsList)
            {
                if (rows) RefreshModRows();
                for (int r = 0; r < g_modRows; ++r)
                {
                    int64_t item[8] = { reinterpret_cast<int64_t>(rows ? g_modRow[r] : g_modHint[r]) };
                    g_addItem(item);
                }
                if (rows) Log(L"mods menu: %zu mod(s) listed", g_mods.size());
                return true;
            }
            if (g_theatre)
            {
                if (rows) RefreshFilmRows();
                for (int r = 0; r < g_filmRows; ++r)
                {
                    int64_t item[8] = { reinterpret_cast<int64_t>(rows ? g_filmRow[r] : g_filmHint[r]) };
                    g_addItem(item);
                }
                if (rows) Log(L"theatre: %d film(s) listed", static_cast<int>(g_films.size()));
                return true;
            }
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
            static constexpr Relabel kToTheatre[] = { { "ONLINE SETTINGS", "THEATRE" } },
                kToMods[] = { { "ONLINE SETTINGS", "MODS" } },
                kToSettings[] = { { "THEATRE", "ONLINE SETTINGS" }, { "MODS", "ONLINE SETTINGS" } };
            RelabelScript(script, kToSettings, std::size(kToSettings));
            if (g_theatre) RelabelScript(script, kToTheatre, 1);
            else if (g_modsList) RelabelScript(script, kToMods, 1);
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
        // A second row (2026-11-14, the user's request): Skill Preview Sound, option 7, the same way.
        constexpr int kAuraOption = 6, kPreviewSoundOption = 7;
        std::atomic<bool> g_soundWindow{ false };
        char g_soundRowText[96], g_previewSoundRowText[96];
        const char* kSoundHint = "Turn the beeping of the aura particles on or off.";
        const char* kPreviewSoundHint = "Turn the sound of the skill preview (Arsenal Lab, View Stock List) on or off.";

        bool InSoundWindow() { return g_soundWindow && CurrentMenu(MenuManager()) == kOptionsMenu; }

        void RefreshSoundRows()
        {
            sprintf_s(g_soundRowText, "\a190wAura Particle Beeps\f\a1#\a150c%s\f\a0#", GetConfig().auraBeeps ? "On" : "Off");
            sprintf_s(g_previewSoundRowText, "\a190wSkill Preview Sound\f\a1#\a150c%s\f\a0#", GetConfig().previewSound ? "On" : "Off");
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
            int64_t preview[8] = { reinterpret_cast<int64_t>(rows ? g_previewSoundRowText : kPreviewSoundHint) };
            g_addItem(preview);
            if (rows)
            {
                g_soundWindow = true;
                Log(L"gamepatch: Options > Sound: Aura Particle Beeps and Skill Preview Sound rows added");
            }
            return true;
        }

        // Options > Send Log (main menu, menu 4): a seventh item in the OPTIONS list, after Keyboard, with its hint after
        // Keyboard's (added as the list is built, before menu 4 counts as current). Each frame the script hands its state
        // to menu command 6 ("aMAIN"), which writes the name of the label to run for that frame's input at manager
        // +0x1062c (FUN_140245f00: state + "_S<item>" when an item is picked). The script has no aMAIN_S6, so the game
        // itself does nothing; HookedSetState sees that name and uploads the logs to log_webhook (discord::SendLogs).
        // The item's text says how it went.
        char g_logRowText[96] = "Send Log";
        const char* kLogHint = "Send this PC's game logs to the developers on Discord.";
        std::atomic<bool> g_sendingLog{ false };
        bool g_logItemReady = false; // menu command 6 is wrapped (SoundOptions)

        // Options > Updates: an eighth item, after Send Log, picked the same way (event 107, "aMAIN_S7"). Each pick goes
        // stable -> experimental -> off -> stable in update_channel (settings.ini), which pdlaunch reads at the next game
        // start (CheckForUpdate; settings.ini overrides pdnet.ini; off: no check at all). Empty means the kit's own kind
        // (off for a development build). Stable is the public version since 2026-11-10: the public channel is gone, and a
        // leftover "public" (a Public kit's pdnet.ini or settings.ini) counts as stable, as in pdlaunch.
        char g_updateRowText[64];
        const char* kUpdateHint = "Builds to update to at game start. Stable: the public release. Experimental: newest, being tested.\nOff: no automatic updates.";

        std::wstring UpdateChannel()
        {
            std::wstring channel = GetConfig().updateChannel;
            if (_wcsicmp(channel.c_str(), L"public") == 0) return L"stable";
            for (const wchar_t* known : { L"stable", L"experimental", L"off" })
                if (_wcsicmp(channel.c_str(), known) == 0) return known;
            std::string label = BuildLabel(); // empty or a typo in settings.ini: the kit's own, as pdlaunch does
            if (label.find("Linux") != std::string::npos) return L"off"; // no pdlaunch there, so no updates
            return label.find("Experimental") != std::string::npos ? L"experimental"
                 : label.find("Stable") != std::string::npos || label.find("Public") != std::string::npos ? L"stable" : L"off";
        }

        // Conceptual kits update only from Conceptual releases and don't use update_channel, which is shared with other kits.
        bool ConceptualKit() { return strstr(BuildLabel(), "Conceptual") != nullptr; }

        void RefreshUpdateRow()
        {
            if (ConceptualKit())
            {
                sprintf_s(g_updateRowText, "Updates: %s", GetConfig().conceptualUpdatesOff ? "Off" : "Conceptual");
                return;
            }
            std::wstring channel = UpdateChannel();
            sprintf_s(g_updateRowText, "Updates: %s", channel == L"experimental" ? "Experimental" : channel == L"off" ? "Off" : "Stable");
        }

        void SwitchUpdates()
        {
            if (ConceptualKit())
            {
                bool off = !GetConfig().conceptualUpdatesOff;
                EditableConfig().conceptualUpdatesOff = off;
                SaveSetting("conceptual_updates", off ? "off" : "on");
                RefreshUpdateRow();
                Log(L"gamepatch: Options > Updates: Conceptual kit updates %s (used from the next game start)", off ? L"off" : L"on");
                return;
            }
            std::wstring now = UpdateChannel();
            std::wstring next = now == L"stable" ? L"experimental" : now == L"experimental" ? L"off" : L"stable";
            EditableConfig().updateChannel = next;
            SaveSetting("update_channel", winrt::to_string(next));
            RefreshUpdateRow();
            Log(L"gamepatch: Options > Updates: %s (used from the next game start)", next.c_str());
        }

        // Laboratory (menu 45, also opened from the lobby): Buy Arsenal Case and Sell Arsenal Case after View Stock List
        // (View Tutorial Movie, id 2, is added only in some modes: not in multiplayer), with the shop's hints. The
        // Laboratory adds its rows with menu command 59 (add item with an id: args[0] id, args[1] text) and their hints
        // with command 40 (HookedAddItem). Its window reports a pick by position (event 100 + position, label
        // aMAIN_S<position>; FUN_14024b000 uses the id only for windows in mode 1), mapped by g_rowAction (CheckSendLogPick);
        // pdnet then opens arsenalbuy (30) / arsenalsell (31) on top the way the script's "open menu" does, and their back
        // returns to the Laboratory, as it does to the shop. (Experimental 79 looked for the rows in command 40: none.)
        const char* kCaseItems[2] = { "Buy Arsenal Case", "Sell Arsenal Case" };
        const char* kCaseHints[2] = { "Buy an arsenal case.\nA case is necessary to create a new arsenal.", "Sell arsenal cases that you don't need." };
        bool CaseItemsReady() { return g_logItemReady && g_pushMenu; } // no items that can't work
        // Test Arsenal: only in the Lab of the multiplayer menu, not the lobby's (a battle from there would break the lobby).
        const char* kTestItem = "Test Arsenal";
        const char* kTestHint = "Battle an AI copy of your arsenal to try it out.\nNo rewards, nothing counts for the scoreboard.";
        bool TestItemWanted() { return CaseItemsReady() && !g_editorFromLobby; }

        bool CaseItems(const char* added, int64_t* args, int64_t& result)
        {
            if (!CaseItemsReady() || strcmp(added, "Display your stock list.") != 0) return false;
            result = g_addItem(args);
            for (const char* hint : kCaseHints)
            {
                int64_t item[8] = { reinterpret_cast<int64_t>(hint) };
                g_addItem(item);
            }
            if (TestItemWanted())
            {
                int64_t item[8] = { reinterpret_cast<int64_t>(kTestHint) };
                g_addItem(item);
            }
            return true;
        }

        // New map slots (Conceptual; docs\CONCEPTUAL.md, "New map slots"). pdlaunch (launcher\mapslots.cpp) gives each map
        // mod that's on a stage entry from 30 up (entry index = map number), copies its files to st<N>.alr / .dat, and
        // writes a stage list with the entries and an MNet.alr with a preview sprite per slot, all as loose files PDPL
        // serves; LocalState\pdnet\map-slots.txt lists them ("slot <entry> <sprite> <mod id> <name>"). The map picker
        // (livestagesel, menu 47) walks a fixed list of entries 2-26 only, so in that menu pdnet:
        // - adds a row per slot (menu command 59: id 100 + entry, the entry's name from the stage table) right after the
        //   script's last row (stage 8, id 108);
        // - answers the stage query's "unlocked" (menu command 87 kind 0x39) with 1 for the slots: their bits (entry + 27)
        //   are past the 56 the game keeps, so they always read locked;
        // - draws the slot's preview (MNet sprite) instead of the script's (menu command 18: sheet 3, sprite, 48, 261)
        //   while the cursor (window field 9, as menu command 55(window, 9) reads it) is on a slot's row;
        // - and at the pick (session command 40, FUN_140213c00: entry -> 0x14051775c, its map number -> 0x140527bc8, a
        //   random layout -> 0x140527c54) with the cursor on a slot's row, passes that slot's entry, whatever the script
        //   worked out from the row.
        // A slot counts only if the game's stage table has it with the multiplayer flag (the stage list came from PDPL).
        MenuCommandFn g_addIdItem = nullptr; // menu command 59 (HookedAddIdItem, below)
        struct MapSlot { int entry; int sprite; };
        std::vector<MapSlot> g_mapSlots;
        std::atomic<int> g_mapWindow{ -1 };   // the window the map picker's rows went into; -1 none yet
        int8_t g_mapRowSlot[256];             // row -> index into g_mapSlots; -1 a script row
        constexpr int kStageEntrySize = 0x28; // the stage table at 0x140509ba0 (+0 name, +0x18 map number, +0x20 flag)

        bool MapSlotInTable(int entry)
        {
            int count = *reinterpret_cast<int32_t*>(At(0x14050a5d0));
            return entry > 0 && entry < count && *reinterpret_cast<int32_t*>(At(0x140509ba0) + entry * kStageEntrySize + 0x20) == 1;
        }

        bool IsMapSlot(int entry)
        {
            for (const MapSlot& s : g_mapSlots)
                if (s.entry == entry) return MapSlotInTable(entry);
            return false;
        }

        // The slot under the map picker's cursor (index into g_mapSlots), or -1.
        int CursorMapSlot()
        {
            int window = g_mapWindow;
            int64_t manager = MenuManager();
            if (window < 0 || CurrentMenu(manager) != kStageMenu) return -1;
            int row = *reinterpret_cast<int16_t*>(manager + window * 0x103c + 0x40 + 9 * 2); // as menu command 55(window, 9)
            return row >= 0 && row < 256 ? g_mapRowSlot[row] : -1;
        }

        // From HookedAddIdItem, after the game added a row (position = the row's index) to window.
        void AddMapSlotRows(int64_t* args, int window, int position)
        {
            if (g_mapSlots.empty() || CurrentMenu(MenuManager()) != kStageMenu || window < 0 || position < 0) return;
            if (position == 0) memset(g_mapRowSlot, -1, sizeof(g_mapRowSlot));
            if (args[0] != 108) return; // stage 8: the script's last row
            int row = position + 1, added = 0;
            for (size_t k = 0; k < g_mapSlots.size() && row < 256; ++k)
            {
                if (!MapSlotInTable(g_mapSlots[k].entry)) continue;
                const char* name = *reinterpret_cast<const char**>(At(0x140509ba0) + g_mapSlots[k].entry * kStageEntrySize);
                int64_t item[8] = { 100 + g_mapSlots[k].entry, reinterpret_cast<int64_t>(name) };
                g_addIdItem(item);
                g_mapRowSlot[row++] = static_cast<int8_t>(k);
                ++added;
            }
            g_mapWindow = window;
            static bool logged = false;
            if (!logged) { logged = true; Log(L"gamepatch: map picker: %d new map(s) added after row %d (window %d)", added, position, window); }
        }

        MenuCommandFn g_stageQuery = nullptr;
        int64_t HookedStageQuery(int64_t* args)
        {
            if (args && args[0] == 0x39 && IsMapSlot(static_cast<int32_t>(args[1]))) return 1;
            return g_stageQuery(args);
        }

        MenuCommandFn g_drawSheetSprite = nullptr;
        int64_t HookedDrawSheetSprite(int64_t* args)
        {
            if (args && args[0] == 3 && args[2] == 48 && args[3] == 261)
            {
                int k = CursorMapSlot();
                if (k >= 0)
                {
                    int64_t a[8] = { args[0], g_mapSlots[k].sprite, args[2], args[3] };
                    return g_drawSheetSprite(a);
                }
            }
            return g_drawSheetSprite(args);
        }

        SessionCommandFn g_stagePick = nullptr;
        int64_t HookedStagePick(int32_t* args)
        {
            int k = args ? CursorMapSlot() : -1;
            if (k >= 0 && MapSlotInTable(g_mapSlots[k].entry))
            {
                int32_t a[8] = { g_mapSlots[k].entry };
                if (args[0] != a[0]) Log(L"gamepatch: map picker: the pick %d is new map %d (the cursor's row)", args[0], a[0]);
                else Log(L"gamepatch: map picker: new map %d picked", a[0]);
                return g_stagePick(a);
            }
            return g_stagePick(args);
        }

        // The match load (FUN_1401d85f0) queues the map's files from a table of names by map number (0x1403f2b60, 0x18 per
        // entry: name, list flag, 0; entries 2N and 2N + 1 are stNN.alr / stNN.dat for N up to 34, blank where a number
        // has no map, then the ambience banks MapSE*.bin from entry 70 and st09_xx from 82) through FUN_14021ab70 (list,
        // entry, buffer). A slot's entries are blank or other files (map 30: entry 60 is empty, so the file loader read
        // a null name and crashed, Conceptual 52), so while the map number (0x140530264) is a slot its two entries are
        // swapped for pdnet's own. The game keeps the entry pointer (0x140533050), so they're never freed.
        struct MapFileEntry { const char* name; int32_t flag; int32_t pad; uint64_t zero; };
        MapFileEntry g_mapFiles[65][2] = {};
        char g_mapFileNames[65][2][12] = {};
        using QueueMapFileFn = void (*)(int64_t list, uint64_t* entry, uint64_t buffer);
        QueueMapFileFn g_queueMapFile = nullptr;
        void HookedQueueMapFile(int64_t list, uint64_t* entry, uint64_t buffer)
        {
            int map = *reinterpret_cast<int32_t*>(At(0x140530264));
            if (map >= 30 && map < 65 && g_mapFiles[map][0].name)
            {
                auto table = reinterpret_cast<uint64_t*>(At(0x1403f2b60));
                for (int f = 0; f < 2; ++f)
                    if (entry == table + (map * 2 + f) * 3)
                    {
                        static int logged = 0;
                        if (logged++ < 8) Log(L"gamepatch: map load: map %d's %S from the new map slot", map, g_mapFiles[map][f].name);
                        entry = reinterpret_cast<uint64_t*>(&g_mapFiles[map][f]);
                    }
            }
            g_queueMapFile(list, entry, buffer);
        }

        void MapSlots()
        {
            memset(g_mapRowSlot, -1, sizeof(g_mapRowSlot));
            if (!GetModuleHandleW(L"pd_loader_core.dll")) return; // no PDPL: the game has its own stage list
            std::ifstream in(LocalStatePath() + L"\\pdnet\\map-slots.txt", std::ios::binary);
            std::string line;
            while (std::getline(in, line))
            {
                int entry = 0, sprite = 0;
                if (sscanf_s(line.c_str(), "slot\t%d\t%d", &entry, &sprite) == 2 && entry >= 30 && entry < 65 && sprite > 0 && sprite < 128)
                    g_mapSlots.push_back({ entry, sprite });
            }
            if (g_mapSlots.empty()) return;
            int count = *reinterpret_cast<int32_t*>(At(0x14050a5d0));
            if (!g_addIdItem)
            {
                Log(L"gamepatch: map picker: %zu new map(s) NOT added (the row hook isn't in)", g_mapSlots.size());
                g_mapSlots.clear();
                return;
            }
            bool hooks = WrapCommand<MenuCommandFn>(0x1403eefe0 + 87 * 8, 0x140208340, &HookedStageQuery, g_stageQuery, L"map picker stage query") &&
                         WrapCommand<MenuCommandFn>(0x1403eefe0 + 18 * 8, 0x140207730, &HookedDrawSheetSprite, g_drawSheetSprite, L"map picker preview") &&
                         WrapCommand<SessionCommandFn>(0x1403f1150 + 40 * 8, 0x140213c00, &HookedStagePick, g_stagePick, L"map picker pick");
            for (const MapSlot& s : g_mapSlots)
                for (int f = 0; f < 2; ++f)
                {
                    sprintf_s(g_mapFileNames[s.entry][f], "st%02d.%s", s.entry, f ? "dat" : "alr");
                    g_mapFiles[s.entry][f].name = g_mapFileNames[s.entry][f];
                }
            // mov [rsp+8], rbx / mov eax, [rdx+8] (the next instruction is RIP-relative)
            std::vector<uint8_t> start = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x8B, 0x42, 0x08 };
            g_queueMapFile = reinterpret_cast<QueueMapFileFn>(InlineHookBytes(0x14021ab70, start, start, reinterpret_cast<void*>(&HookedQueueMapFile), L"map load files"));
            hooks = hooks && g_queueMapFile;
            Log(L"gamepatch: map picker: %zu new map(s) from map-slots.txt, stage table has %d entries%s", g_mapSlots.size(), count, hooks ? L"" : L" - NOT all hooks in");
        }

        // What each row of a command-59 list does when picked, by window and position. The Laboratory's window reports
        // a pick by POSITION (event 100 + position, label aMAIN_S<position>; its labels are aMAIN_S0-S2 for its rows
        // with ids 0-2), so with the case rows in between a later row moves down: its pick is mapped back to its
        // label (aMAIN_S<id>), and the case rows' picks open the case menus (Experimental 82 sent Buy to the movie list).
        constexpr int kWindows = 16, kRows = 32;
        constexpr int8_t kRowBuy = 100, kRowSell = 101, kRowTest = 102;
        int8_t g_rowAction[kWindows][kRows]; // the row's id (its label number), kRowBuy / kRowSell; -1 unknown

        int64_t HookedAddIdItem(int64_t* args)
        {
            int64_t manager = MenuManager();
            int window = manager ? *reinterpret_cast<int16_t*>(manager + 0x10402) : -1;
            bool tracked = window >= 0 && window < kWindows;
            int position = tracked ? *reinterpret_cast<int16_t*>(manager + window * 0x103c + 0x56) : 0; // rows so far (FUN_140246770)
            if (tracked && position == 0) memset(g_rowAction[window], -1, kRows);
            int64_t result = g_addIdItem(args);
            if (!args) return result;
            AddMapSlotRows(args, window, tracked ? position : -1);
            if (tracked && position < kRows) g_rowAction[window][position] = static_cast<int8_t>(args[0] >= 0 && args[0] < kRowBuy ? args[0] : -1);
            char text[32];
            if (!CaseItemsReady() || !CopyString(reinterpret_cast<const char*>(args[1]), text, sizeof(text)) || strcmp(text, "View Stock List") != 0) return result;
            for (int i = 0; i < 2; ++i)
            {
                int64_t item[8] = { 3 + i, reinterpret_cast<int64_t>(kCaseItems[i]) };
                g_addIdItem(item);
                if (tracked && position + 1 + i < kRows) g_rowAction[window][position + 1 + i] = i == 0 ? kRowBuy : kRowSell;
            }
            if (TestItemWanted())
            {
                int64_t item[8] = { 5, reinterpret_cast<int64_t>(kTestItem) };
                g_addIdItem(item);
                if (tracked && position + 3 < kRows) g_rowAction[window][position + 3] = kRowTest;
            }
            Log(L"gamepatch: Arsenal Lab: Buy / Sell Arsenal Case items added (rows %d and %d)%s", position + 1, position + 2, TestItemWanted() ? L", Test Arsenal (row after)" : L"");
            return result;
        }

        // Dynamic arsenal cases. An arsenal's school number, case +0x50, does two things:
        // - the editor refuses more schools than it (session command at 0x140216cc0: schools used > editing arsenal's +0x50);
        // - at a match start it picks the Aura refill rate from gsdata +0x33034: 1 school 1.3, 2 schools 1.0, 3 schools 0.7
        //   (FUN_140187710, FUN_14012a750, used in FUN_14024de20). Other players' games use the number in the arsenal this
        //   PC sends, so a player without this kit sees the right rate too.
        // - the screens show it as the arsenal's type (FUN_14016d5f0: picture 0x7e + the number).
        // So in the multiplayer profile the number always follows the arsenal, 1 to 3, also while the editor is open, as a
        // visual indicator; HookedSchoolLimit keeps the editor's limit at 3 whatever the number says. The story profile is
        // left alone. An empty arsenal counts as 1, so a first skill doesn't change its number.
        // Schools: skill record DAT_1404bdc50 + id * 0x90, +0x12, above 0 (as FUN_14015b830); Aura Particles have none.
        constexpr int kMaxCaseSchools = 3;

        int SchoolsInArsenal(const uint8_t* arsenal, const uint8_t* skills)
        {
            uint32_t seen = 0;
            for (int i = 0; i < 30; ++i)
            {
                int16_t id = *reinterpret_cast<const int16_t*>(arsenal + 0x14 + i * 2);
                if (id < 0) continue;
                int16_t school = *reinterpret_cast<const int16_t*>(skills + 0x12 + id * 0x90);
                if (school > 0 && school < 32) seen |= 1u << school;
            }
            int count = 0;
            for (; seen; seen &= seen - 1) ++count;
            return count;
        }

        int CaseNumberFor(const uint8_t* arsenal, const uint8_t* skills, int& schools)
        {
            schools = SchoolsInArsenal(arsenal, skills);
            return schools < 1 ? 1 : schools > kMaxCaseSchools ? kMaxCaseSchools : schools;
        }

        // The arsenal the editor is working on, wherever it holds it: DAT_1404c0140 +0x10a0 (FUN_14016fe20) and +0x4a8
        // (FUN_14016d460, FUN_140217620).
        void EditedArsenalsFollow(const uint8_t* skills)
        {
            uint8_t* editor = *reinterpret_cast<uint8_t**>(At(0x1404c0140));
            uint8_t* held[2] = { editor ? *reinterpret_cast<uint8_t**>(editor + 0x10a0) : nullptr, editor ? *reinterpret_cast<uint8_t**>(editor + 0x4a8) : nullptr };
            for (uint8_t* arsenal : held)
            {
                if (!arsenal) continue;
                int schools = 0;
                int wanted = CaseNumberFor(arsenal, skills, schools);
                auto& number = *reinterpret_cast<int16_t*>(arsenal + 0x50);
                if (number != wanted) number = static_cast<int16_t>(wanted);
            }
        }

        void CasesFollowArsenals(bool editing)
        {
            __try
            {
                uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
                uint8_t* skills = *reinterpret_cast<uint8_t**>(At(0x1404bdc50));
                if (!save || !skills || !MultiplayerProfile()) return;
                uint8_t* list = *reinterpret_cast<uint8_t**>(save + 0x10a * 0x38);
                int cases = *reinterpret_cast<int32_t*>(save + 0x3a48);
                for (int i = 0; list && i < cases && i < 16; ++i)
                {
                    uint8_t* arsenal = list + i * 100;
                    int schools = 0;
                    int wanted = CaseNumberFor(arsenal, skills, schools);
                    auto& number = *reinterpret_cast<int16_t*>(arsenal + 0x50);
                    if (number == wanted) continue;
                    if (!editing) Log(L"gamepatch: arsenal case %d: %d school(s) in it, number %d (was %d)", i, schools, wanted, number);
                    number = static_cast<int16_t>(wanted);
                }
                if (editing) EditedArsenalsFollow(skills);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {}
        }

        // The editor's "too many schools" check when saving (session command at 0x140216cc0). The game compares counters with
        // the arsenal's number, but those counters weren't current when tested, so here the schools are counted in the arsenal
        // itself. Arsenals made before the limit can still reach it; HookedSetSkill stops new ones.
        NativeFn g_schoolLimit = nullptr;
        int64_t HookedSchoolLimit(int64_t* args)
        {
            if (!MultiplayerProfile()) return g_schoolLimit(args);
            __try
            {
                uint8_t* editor = *reinterpret_cast<uint8_t**>(At(0x1404c0140));
                uint8_t* skills = *reinterpret_cast<uint8_t**>(At(0x1404bdc50));
                uint8_t* held[2] = { editor ? *reinterpret_cast<uint8_t**>(editor + 0x10a0) : nullptr, editor ? *reinterpret_cast<uint8_t**>(editor + 0x4a8) : nullptr };
                int used = 0;
                for (uint8_t* arsenal : held)
                    if (arsenal && skills) used = (std::max)(used, SchoolsInArsenal(arsenal, skills));
                Log(L"gamepatch: arsenal save check: %d school(s), limit %d", used, kMaxCaseSchools);
                return used > kMaxCaseSchools ? 1 : 0;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return 0;
            }
        }

        // The editor putting a skill in a slot, FUN_140159db0(state, skill, c): state +0x48 is the arsenal; skill 0 empties
        // the slot; below 0 takes the skill selected in the list (state +0x278 the list, +0x27c + list * 0x18 the row,
        // DAT_1404c0140 +0x138c + ((list - 1) * 0x3ea + row) * 2). It replaces the slot's skill, state +0x84 + index * 8, index
        // from state +0x174 + (state +0x268) * 8. Below 0 is its own refusal: -3 ("more than 3 of the same skill") makes the
        // caller set state +0xb38 = 10, and the editor's script (arsenaledit, English) then shows its pop-up. So a skill that
        // would make a 4th school returns -3 too, with the pop-up's text swapped first; it's swapped back before any other try.
        using SetSkillFn = int (*)(uint8_t* state, int skill, int c);

        bool WouldPassSchoolLimit(uint8_t* state, int skill)
        {
            __try
            {
                uint8_t* editor = *reinterpret_cast<uint8_t**>(At(0x1404c0140));
                uint8_t* skills = *reinterpret_cast<uint8_t**>(At(0x1404bdc50));
                uint8_t* arsenal = *reinterpret_cast<uint8_t**>(state + 0x48);
                if (!editor || !skills || !arsenal) return false;
                if (skill < 0)
                {
                    int list = *reinterpret_cast<int32_t*>(state + 0x278);
                    if (list < 1) return false;
                    int row = *reinterpret_cast<int32_t*>(state + 0x27c + list * 0x18);
                    skill = *reinterpret_cast<int16_t*>(editor + 0x138c + ((list - 1) * 0x3ea + row) * 2);
                }
                if (skill <= 0) return false;
                int16_t school = *reinterpret_cast<int16_t*>(skills + 0x12 + skill * 0x90);
                if (school <= 0 || school >= 32) return false;
                int slot = *reinterpret_cast<int32_t*>(state + 0x174 + *reinterpret_cast<int32_t*>(state + 0x268) * 8);
                int16_t replaced = slot < 0 ? -1 : *reinterpret_cast<int16_t*>(state + 0x84 + slot * 8);
                if (replaced <= 0) replaced = -1;
                uint32_t seen = 1u << school;
                bool skipped = false;
                for (int i = 0; i < 30; ++i)
                {
                    int16_t id = *reinterpret_cast<int16_t*>(arsenal + 0x14 + i * 2);
                    if (id < 0) continue;
                    if (id == replaced && !skipped) { skipped = true; continue; }
                    int16_t other = *reinterpret_cast<int16_t*>(skills + 0x12 + id * 0x90);
                    if (other > 0 && other < 32) seen |= 1u << other;
                }
                int count = 0;
                for (; seen; seen &= seen - 1) ++count;
                return count > kMaxCaseSchools;
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                return false;
            }
        }

        void SwapRefusalText(bool schools)
        {
            static constexpr const char* kSameSkill = "You cannot place more than 3 of the same \nskill in an arsenal.";
            static constexpr const char* kSchools = "You cannot use more than 3 schools \nin an arsenal."; // not longer: it's written over the other
            uint8_t* script = *reinterpret_cast<uint8_t* volatile*>(At(0x1403fa3d0) + kEditorMenu * 0x18 + 0x10);
            if (!script) return;
            const Relabel swap = schools ? Relabel{ kSameSkill, kSchools } : Relabel{ kSchools, kSameSkill };
            RelabelScript(script, &swap, 1);
        }

        int HookedSetSkill(uint8_t* state, int skill, int c)
        {
            if (skill != 0 && MultiplayerProfile() && WouldPassSchoolLimit(state, skill))
            {
                Log(L"gamepatch: arsenal editor: skill refused, a school past the limit of %d", kMaxCaseSchools);
                SwapRefusalText(true);
                return -3;
            }
            SwapRefusalText(false);
            return reinterpret_cast<SetSkillFn>(At(0x140159db0))(state, skill, c);
        }

        int64_t HookedDrawText(int64_t* args); // (below)

        // The shop (menu 6) builds its stock as it opens, with script group 3 #310 (prices and stock, FUN_1401ce100) or #347
        // (stock, FUN_1401cdef0); the case list (30) then shows bit N of 0x14052ad0e as a case for N + 1 schools. In the
        // multiplayer profile only the first stays: one plain case, as in the Lab.
        NativeFn g_shopStock = nullptr, g_shopStock2 = nullptr;
        void OneCaseInStock() { if (MultiplayerProfile()) *At(0x14052ad0e) = 1; }
        int64_t HookedShopStock(int64_t* args) { int64_t r = g_shopStock(args); OneCaseInStock(); return r; }
        int64_t HookedShopStock2(int64_t* args) { int64_t r = g_shopStock2(args); OneCaseInStock(); return r; }

        // The buy list's row "%d-School Arsenal Case\a346r\a1f%d\a0f credits" (schools, price) is drawn as "Arsenal Case ..."
        // with the price only. True when `changed` holds the call to make instead.
        bool CaseRowText(const int64_t* args, int64_t* changed)
        {
            static constexpr char kRow[] = "%d-School Arsenal Case";
            if (!args || CurrentMenu(MenuManager()) != kCaseBuyMenu || !MultiplayerProfile()) return false;
            char format[96];
            if (!CopyString(reinterpret_cast<const char*>(args[2]), format, sizeof(format)) || strncmp(format, kRow, sizeof(kRow) - 1) != 0) return false;
            static char text[96];
            snprintf(text, sizeof(text), "Arsenal Case%s", format + sizeof(kRow) - 1);
            changed[0] = args[0];
            changed[1] = args[1];
            changed[2] = reinterpret_cast<int64_t>(text);
            for (int i = 3; i < 7; ++i) changed[i] = args[i + 1]; // the school number dropped
            changed[7] = 0;
            return true;
        }

        // An arsenal's panel (FUN_14016d5f0: case picture, name, skills) draws "<prefix><slot, 2 digits>" above the name when its
        // 6th argument is 0 or more, and uses that argument for nothing else. The slot number means nothing to players (the
        // user), so the three calls pass -1 and the label isn't drawn.
        using ArsenalPanelFn = void (*)(int64_t a, int x, int y, int64_t arsenal, int64_t e, int slot, int colour, int h);
        void HookedArsenalPanel(int64_t a, int x, int y, int64_t arsenal, int64_t e, int slot, int colour, int h)
        {
            reinterpret_cast<ArsenalPanelFn>(At(0x14016d5f0))(a, x, y, arsenal, e, -1, colour, h);
        }

        void DynamicCases()
        {
            int panels = 0;
            for (uint64_t site : { 0x14015c683ull, 0x14016d5c8ull, 0x1401704baull })
                panels += HookCall(site, 0x14016d5f0, reinterpret_cast<void*>(&HookedArsenalPanel)) ? 1 : 0;
            Log(L"gamepatch: arsenal slot label hidden on %d of 3 panels", panels);
            if (!g_drawText) WrapCommand<MenuCommandFn>(0x1403eefe0 + 23 * 8, 0x140207840, &HookedDrawText, g_drawText, L"arsenal case row text");
            WrapCommand<NativeFn>(0x1403ef390 + 310 * 8, 0x14020bd40, &HookedShopStock, g_shopStock, L"shop case stock");
            WrapCommand<NativeFn>(0x1403ef390 + 347 * 8, 0x14020c3d0, &HookedShopStock2, g_shopStock2, L"shop case stock (2)");
            bool limit = WrapCommand<NativeFn>(0x1403f1e48, 0x140216cc0, &HookedSchoolLimit, g_schoolLimit, L"arsenal school limit");
            bool add = true;
            for (uint64_t site : { 0x140159434ull, 0x14015948full, 0x140159be4ull })
                add = HookCall(site, 0x140159db0, reinterpret_cast<void*>(&HookedSetSkill)) && add;
            std::thread([]
            {
                for (;;)
                {
                    Sleep(50);
                    // 0 or 13: not in a match. In the Lab's own screens too: since the skill preview (Experimental 135)
                    // the editor runs on top of pdnet's town session, game state 6, and the arsenal being edited kept
                    // its old number, e.g. "Limit[1]" with 2 schools in it (2026-11-11, Stable 12, MojoJoey).
                    int32_t state = *reinterpret_cast<volatile int32_t*>(At(0x1404bc950));
                    int menu = CurrentMenu(MenuManager());
                    if (state == 0 || state == 13 || InLabMenus(menu)) CasesFollowArsenals(menu == kEditorMenu);
                }
            }).detach();
            Log(L"gamepatch: dynamic arsenal cases on (limit of %d schools: when adding %s, when saving %s)", kMaxCaseSchools, add ? L"ready" : L"NOT ready", limit ? L"ready" : L"NOT ready");
        }

        void OpenCaseMenu(int menu)
        {
            Log(L"gamepatch: Arsenal Lab: %s Arsenal Case picked", menu == kCaseBuyMenu ? L"Buy" : L"Sell");
            // The case screens only show the shop's stock, which the shop builds as it opens: the multiplayer menu sets
            // menu variable 2 to 768 and the shop's InitMenu then calls g3 #310 = FUN_1401ce100 (copies the multiplayer
            // prices from the loaded data at *0x1404c5228 + 0x33c04 and builds the stock lists). Without it the buy
            // screen had no cases (Experimental 83).
            if (*reinterpret_cast<uint64_t*>(At(0x1404c5228)) && memcmp(At(0x1401ce100), "\x48\x83\xec\x28\x48\x8b\x05", 7) == 0)
                reinterpret_cast<void (*)()>(At(0x1401ce100))();
            else Log(L"gamepatch: Arsenal Lab: shop stock not built - the game code or data is not what this pdnet expects");
            // Dynamic cases: one plain case to buy (bit N of the stock = a case for N + 1 schools, FUN_1401c40b0).
            if (menu == kCaseBuyMenu && MultiplayerProfile()) *At(0x14052ad0e) = 1;
            int64_t manager = MenuManager();
            *At(0x1404bbc34) = 0; // as FUN_140207020 (the script's "open menu")
            g_pushMenu(manager, menu, 0);
            // After the state script (command 6 runs in it), the frame's menu update (FUN_140248230) runs the picked
            // label (+0x1062c, "aMAIN_S3") on the CURRENT menu's script: now the case menu, whose script is still
            // loading (null) - a crash in FUN_1402ab5d0 (Experimental 81). No label: nothing runs.
            *reinterpret_cast<char*>(manager + 0x1062c) = 0;
        }

        // Lab > Test Arsenal: the scoreboard battle against an AI, as liveranking starts it once a player's "game clip" is
        // downloaded (g4 #659 = FUN_140183e80): that copies the picked row (0x4c bytes at 0x14051d4c0 + row * 0x4c: id,
        // a number, the name in 0x40 bytes) to 0x14050d230.. and decodes the row's arsenal text into the opponent's
        // 100-byte arsenal record at 0x1404bc8e0 (FUN_1400bf950: same layout as the save's arsenals), status
        // 0x140519abc = 3 (g4 #660 reads it). Here the opponent is a copy of this player's current arsenal (save
        // +0x3b08 into the list at *(save + 0x3a30), 100 bytes each). g4 #575 stores the arsenal to use (0x1403de844).
        // livedbtl (74) reads its menu parameter 0 (command 111/110: word at 0x1403fa3d8 + menu * 0x18 + slot * 2):
        // 0 = open the arsenal choice (67) first, 1 = back from it: start the battle.
        // The opponent: a copy of the save's arsenal `index` (100 bytes, list at *(save + 0x3a30); an entry with byte +4
        // == 0 is empty), named "Test Opponent", marked downloaded.
        bool CopyArsenalToOpponent(int index)
        {
            uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
            uint8_t* arsenals = save ? *reinterpret_cast<uint8_t**>(save + 0x3a30) : nullptr;
            if (!arsenals || index < 0 || index >= 16 || arsenals[index * 100 + 4] == 0)
            {
                Log(L"gamepatch: Test Arsenal: arsenal %d isn't there", index);
                return false;
            }
            memcpy(At(0x1404bc8e0), arsenals + index * 100, 100);
            *reinterpret_cast<uint64_t*>(At(0x14050d270)) = 1; // the row's id (not 0: a player is there)
            *reinterpret_cast<int32_t*>(At(0x14050d278)) = 0;
            char* name = reinterpret_cast<char*>(At(0x14050d230));
            memset(name, 0, 0x40);
            strcpy_s(name, 0x40, "Test Opponent");
            *reinterpret_cast<int32_t*>(At(0x140519abc)) = 3; // "downloaded"
            Log(L"gamepatch: Test Arsenal: opponent = a copy of arsenal %d", index);
            return true;
        }

        // Theatre: a film plays in a Test Arsenal battle started straight away (no picks): the film's stage and layout, its
        // players' characters when the film recorded them (version 2), this player's current arsenal on both sides for now.
        // labtests plays the film once the battle starts.
        // The scoreboard battle (FUN_140214770) builds each side's deck counts (DAT_140585fe8[slot]) with FUN_14014f830
        // from 30 skill ids; the battle loads its skills from those. For a film, both decks are the film players' own, so
        // every skill the film uses is loaded (Conceptual 29 crashed on an effect from a skill this battle hadn't loaded).
        using DeckFn = void (*)(uint64_t, int, const int16_t*);
        void HookedDeck(uint64_t a, int slot, const int16_t* skills)
        {
            int16_t film[30];
            if (g_testBattle && labtests::FilmDeck(slot, film))
            {
                Log(L"theatre: slot %d plays with the film's deck (%d %d %d %d ...)", slot, film[0], film[1], film[2], film[3]);
                skills = film;
            }
            reinterpret_cast<DeckFn>(At(0x14014f830))(a, slot, skills);
        }

        void ApplyFilmVariants(const film::Info& f, int (&characters)[2]); // (below, with the variants)
        int g_filmCharacterOverride[4] = { -1, -1, -1, -1 }; // slots 2-3 of a film: the character to use instead of the film's
        void StartFilmBattle(const film::Info& f)
        {
            uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
            int index = save ? *reinterpret_cast<int32_t*>(save + 0x3b08) : -1;
            if (!labtests::QueueFilm(f.file) || !CopyArsenalToOpponent(index))
            {
                Log(L"theatre: the film's battle couldn't start");
                return;
            }
            int characters[2] = { f.character[0], f.character[1] }; // each film player keeps their slot
            ApplyFilmVariants(f, characters);
            *reinterpret_cast<int32_t*>(At(0x1403de844)) = index;
            g_testBattle = true;
            g_testStage = f.stage;
            g_testLayout = f.layout;
            g_testCharacter = characters[0];
            g_testOpponentCharacter = characters[1];
            g_testArsenal = index;
            g_testPicking = kPickingNone;
            LoadMultiplayerTextures();
            ResetLabVariables();
            MenuParam(kScoreBattleMenu, 0) = 1; // livedbtl: start the battle
            int64_t manager = MenuManager();
            *At(0x1404bbc34) = 0;
            g_pushMenu(manager, kScoreBattleMenu, 0);
            *reinterpret_cast<char*>(manager + 0x1062c) = 0;
            Log(L"theatre: battle for the film: stage %d, layout %d, characters %d / %d", f.stage, f.layout, characters[0], characters[1]);
        }

        void OpenTestArsenal()
        {
            uint8_t* save = *reinterpret_cast<uint8_t**>(At(0x1404bec10));
            int index = save ? *reinterpret_cast<int32_t*>(save + 0x3b08) : -1;
            if (!CopyArsenalToOpponent(index)) // the current arsenal until the opponent's is picked
            {
                Log(L"gamepatch: Test Arsenal: no current arsenal - not started");
                return;
            }
            *reinterpret_cast<int32_t*>(At(0x1403de844)) = index; // as g4 #575
            g_testBattle = true;
            g_testStage = -1;
            g_testCharacter = -1;
            g_testOpponentCharacter = -1;
            g_testArsenal = index;
            LoadMultiplayerTextures();
            *reinterpret_cast<int32_t*>(At(0x140527bc0)) = -1;
            MenuVariables()[0] = 1;
            g_testPicking = kPickingCharacter;
            Log(L"gamepatch: Test Arsenal: choose your character (menu %d)", kCharacterMenu);
            int64_t manager = MenuManager();
            *At(0x1404bbc34) = 0;
            g_pushMenu(manager, kCharacterMenu, 0);
            *reinterpret_cast<char*>(manager + 0x1062c) = 0; // no picked label on the new menu (see OpenCaseMenu)
        }

        // Opening livearsel on the frame of the A press that picked Test Arsenal let that press confirm its first arsenal as
        // soon as it loaded (Experimental 89: the opponent screen closed by itself). Test Arsenal opens it 0.3 s later, on a
        // frame with no input, while the Lab is still on screen.
        std::atomic<ULONGLONG> g_testPendingAt{ 0 };
        void OpenPendingTestArsenal(int32_t event)
        {
            ULONGLONG at = g_testPendingAt;
            if (!at) return;
            if (CurrentMenu(MenuManager()) != kLabMenu) { g_testPendingAt = 0; return; }
            if (event != 0 || GetTickCount64() - at < 300) return;
            g_testPendingAt = 0;
            OpenTestArsenal();
        }

        bool LogItems(const char* added, int64_t* args, int64_t& result)
        {
            // Only the Options scripts add $KEYBOARD items. No menu check: the list is built before menu 4 is current.
            if (!g_logItemReady) return false; // no item that can't work
            bool row = strcmp(added, "$KEYBOARD") == 0, hint = strcmp(added, "$KEYBOARD_HINT") == 0;
            if (!row && !hint) return false;
            result = g_addItem(args);
            RefreshUpdateRow();
            int64_t item[8] = { reinterpret_cast<int64_t>(row ? g_logRowText : kLogHint) };
            g_addItem(item);
            int64_t updates[8] = { reinterpret_cast<int64_t>(row ? g_updateRowText : ConceptualKit() ? "Updates this Conceptual kit at game start, from Conceptual builds only.\nOff: no automatic updates." : kUpdateHint) };
            g_addItem(updates);
            if (row) Log(L"gamepatch: Options > Send Log and Updates items added");
            return true;
        }

        void SendLog()
        {
            if (g_sendingLog.exchange(true)) return;
            strcpy_s(g_logRowText, "Sending Log...");
            Log(L"gamepatch: Options > Send Log picked");
            std::thread([]
            {
                std::string text = "Log from **" + discord::Name(winrt::to_string(GetConfig().displayName)) + "** - " + BuildLabel();
                std::string why = discord::SendLogs(text);
                if (why.empty()) strcpy_s(g_logRowText, "Log Sent");
                else sprintf_s(g_logRowText, "Log Not Sent: %s", why.c_str());
                Log(L"gamepatch: Options > Send Log: %S", why.empty() ? "sent" : why.c_str());
                g_sendingLog = false;
            }).detach();
        }

        // After menu command 6: the label this frame's input runs. `event` is that input (manager +0x10414, read before
        // command 6; 100 + item for a pick), since the name stays from an earlier frame when there is none. No script
        // has an aMAIN_S6 label (checked 2026-10-29).
        void CheckSendLogPick(int32_t event)
        {
            int64_t manager = MenuManager();
            if (event >= 100 && event < 100 + kRows && manager && CurrentMenu(manager) == kLabMenu && g_pushMenu)
            {
                // The Laboratory's main list (state MAIN): see g_rowAction.
                char label[16];
                int window = *reinterpret_cast<int32_t*>(manager + 0x10418), position = event - 100;
                if (window < 0 || window >= kWindows || !CopyString(reinterpret_cast<const char*>(manager + 0x1062c), label, sizeof(label)) ||
                    strncmp(label, "aMAIN_S", 7) != 0 || atoi(label + 7) != position)
                    return;
                int action = g_rowAction[window][position];
                if (action == kRowBuy || action == kRowSell || action == kRowTest)
                {
                    static ULONGLONG lastCase = 0;
                    if (GetTickCount64() - lastCase < 500) return;
                    lastCase = GetTickCount64();
                    if (action == kRowTest) g_testPendingAt = GetTickCount64(); // opened a moment later (HookedSetState)
                    else OpenCaseMenu(action == kRowBuy ? kCaseBuyMenu : kCaseSellMenu);
                }
                else if (action >= 0 && action != position)
                    sprintf_s(reinterpret_cast<char*>(manager + 0x1062c), 12, "aMAIN_S%d", action); // the row's own label (12: the game's strncpy size)
                return;
            }
            if ((event != 106 && event != 107) || !manager || CurrentMenu(manager) != kOptionsMenu) return;
            char label[16];
            if (!CopyString(reinterpret_cast<const char*>(manager + 0x1062c), label, sizeof(label)) || _stricmp(label, event == 106 ? "aMAIN_S6" : "aMAIN_S7") != 0) return;
            static ULONGLONG last[2] = {};
            ULONGLONG& at = last[event - 106];
            if (GetTickCount64() - at < (event == 106 ? 3000u : 300u)) return;
            at = GetTickCount64();
            if (event == 106) SendLog();
            else SwitchUpdates();
        }

        MenuCommandFn g_getOption = nullptr, g_setOption = nullptr, g_setState = nullptr, g_setState12 = nullptr;
        int64_t HookedGetOption(int64_t* args)
        {
            if (args && (args[0] == kAuraOption || args[0] == kPreviewSoundOption) && InSoundWindow()) return 50;
            return g_getOption(args);
        }

        int64_t HookedSetOption(int64_t* args)
        {
            if (!args || (args[0] != kAuraOption && args[0] != kPreviewSoundOption) || !InSoundWindow()) return g_setOption(args);
            bool aura = args[0] == kAuraOption;
            bool& on = aura ? EditableConfig().auraBeeps : EditableConfig().previewSound;
            on = !on;
            SaveSetting(aura ? "aura_beeps" : "preview_sound", on ? "on" : "off");
            RefreshSoundRows();
            Log(L"gamepatch: Options > Sound: %s %s", aura ? L"aura particle beeps" : L"skill preview sound", on ? L"on" : L"off");
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
        int64_t HookedSetState(int64_t* args)
        {
            NoteState(args);
            int64_t manager = MenuManager();
            if (manager && CurrentMenu(manager) == kCharacterMenu && g_forceBack.exchange(false))
            {
                *reinterpret_cast<int32_t*>(manager + 0x10414) = 6; // the B event: the script runs its back (the Advanced list's pick is stored)
                Log(L"gamepatch: Advanced list: leaving the character select");
            }
            if (manager && CurrentMenu(manager) == kCharacterMenu && g_forceConfirm != kConfirmNone)
            {
                char state[16] = "?";
                if (args) CopyString(reinterpret_cast<const char*>(args[0]), state, sizeof(state));
                int step = g_forceConfirm;
                if (step == kConfirmGrid || step == kConfirmColours)
                {
                    *reinterpret_cast<int32_t*>(manager + 0x10414) = 5;                         // the A event
                    *reinterpret_cast<int32_t*>(manager + 0x10418) = step == kConfirmGrid ? 12 : 2; // on the grid (window 12) / COLORS (window 2)
                    g_forceConfirm = step == kConfirmGrid ? kConfirmWaitColours : kConfirmNone;
                    Log(L"variants: Advanced list: A for the script on %s (state label '%S')", step == kConfirmGrid ? L"the grid" : L"its COLORS window", state);
                }
                else if (GetTickCount64() - g_forceConfirmSince > 5000)
                {
                    Log(L"variants: Advanced list: WARNING the script's COLORS window didn't come within 5 s (state label '%S') - given up", state);
                    g_forceConfirm = kConfirmNone;
                    g_pendingPick = -1;
                }
            }
            int32_t event = manager ? *reinterpret_cast<int32_t*>(manager + 0x10414) : 0;
            int64_t result = g_setState(args);
            CheckSendLogPick(event);
            OpenPendingTestArsenal(event);
            return result;
        }
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

        bool PreviewSessionOn(); // pdnet's town session under the Lab is there (below)

        bool SoundOff(bool positional, int64_t bank, int64_t cue, int64_t sound, uint64_t caller)
        {
            const Config& c = GetConfig();
            // Skill Preview Sound off: while pdnet's town session runs under the Lab, its positional sounds (the skills,
            // the town's ambience emitters) don't start; the menus' own sounds are 2D and still play.
            if (PreviewSessionOn())
            {
                bool blocked = positional && !c.previewSound;
                // Diagnostics (2026-11-16, DESKTOP-SUFDHSP on 170 with the option Off: "sound plays sometimes, also makes
                // the menu noise choppy"): every distinct sound started while the session runs, once, with its caller
                // and whether it was blocked - to tell the preview's sounds from the menu's.
                static std::mutex lock;
                static std::set<std::tuple<bool, int64_t, int64_t, uint64_t>> seen;
                bool fresh = false;
                {
                    std::lock_guard guard(lock);
                    if (seen.size() < 200) fresh = seen.emplace(positional, static_cast<int32_t>(bank), static_cast<int32_t>(cue), caller).second;
                }
                if (fresh)
                    Log(L"sound: during the skill preview: %s bank %lld cue %lld sound %lld from 0x%llX (menu %d)%s", positional ? L"3D" : L"2D",
                        static_cast<int64_t>(static_cast<int32_t>(bank)), static_cast<int64_t>(static_cast<int32_t>(cue)), static_cast<int64_t>(static_cast<int32_t>(sound)),
                        caller, CurrentMenu(MenuManager()), blocked ? L" - blocked (Skill Preview Sound off)" : L"");
                if (blocked) return true;
            }
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
            g_logItemReady = g_setState != nullptr;
            memset(g_rowAction, -1, sizeof(g_rowAction));
            bool caseRows = WrapCommand<MenuCommandFn>(0x1403eefe0 + 59 * 8, 0x140207ec0, &HookedAddIdItem, g_addIdItem, L"Arsenal Lab rows");
            Log(L"gamepatch: Arsenal Lab Buy / Sell Arsenal Case %s", caseRows && CaseItemsReady() ? L"ready" : L"NOT added");
            Log(L"gamepatch: Options > Send Log %s (log_webhook %s)", g_logItemReady ? L"ready" : L"NOT added", GetConfig().logWebhook.empty() ? L"not set" : L"set");
        }

        // B to get out of what otherwise can only be waited for. (Signing in, searching, joining and creating a session have
        // the game's own Cancel.)
        // - Start-up logos: LogoWait, through PatchLogoWait. Runs from pdnet's start: the logos are over by the time Apply runs.
        // - Movies: 0x14050e123 is the game's "stop this movie" flag (FUN_1401bdb00 -> FUN_1401bde70, also used when a file
        //   won't open; FUN_1401bcbe0 clears it); 0x1405b1790 is the playing movie.
        // - The load into a match: only once it has hung, as leaving a load that's still going could hang the others. The
        //   lobby's LOADING screen (HookedButtonBar) or game states 1-5 (normally 3.5 s: 2, then a 90-frame 5) for
        //   kHungLoadMs. Still in the lobby: out through menu 84, as a lost lobby is, which leaves the session and ends
        //   in the multiplayer menu. In the match: the pause menu's Give Up (FUN_140211940), which ends in the lobby.
        constexpr ULONGLONG kHungLoadMs = 30000;
        bool CancelHeld()
        {
            int64_t input[2] = { 0, 0 };
            return (reinterpret_cast<InputFn>(At(0x1402168a0))(input) & 0x2020) != 0;
        }

        // Words in playdatasel_en.ssb's wait routine: `if (arg 1) i = g10 #17 (0)` -> `i = session command 154 (i, arg 1)`.
        bool PatchLogoWait(uint8_t* script)
        {
            __try
            {
                auto code = reinterpret_cast<uint32_t*>(script);
                if (code[0x844 / 4] == 0x42B20201) return true;
                if (code[0x83C / 4] != 0x00010009 || code[0x840 / 4] != 0x00040207 || code[0x844 / 4] != 0x0000000E ||
                    code[0x848 / 4] != 0xA0110101 || code[0x84C / 4] != 0x00000010 || code[0x850 / 4] != 0xFFFF000A) return false;
                code[0x83C / 4] = 0xFFFF0009; // push i
                code[0x840 / 4] = 0x00010009; // push arg 1
                code[0x844 / 4] = 0x42B20201; // call session command 154, 2 arguments
                code[0x848 / 4] = 0x00000010; // push the result
                code[0x84C / 4] = 0xFFFF000A; // i =
                code[0x850 / 4] = 0x00000007; // jump to the next word
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        void BackButtonTick(int64_t manager)
        {
            int current = CurrentMenu(manager);
            static int32_t lastAssigning = -1;
            int32_t assigning = *reinterpret_cast<int32_t*>(At(0x1403dd3d8));
            if (assigning != lastAssigning)
            {
                lastAssigning = assigning;
                Log(L"gamepatch: keyboard: assigning row %d (menu %d)", assigning, current);
            }
            if (current == kStartMenu && g_optionChanged)
            {
                static uint8_t* patched = nullptr;
                uint8_t* script = *reinterpret_cast<uint8_t**>(At(0x1403fa3d0 + kStartMenu * 0x18 + 0x10));
                if (script && script != patched)
                {
                    patched = script;
                    Log(L"gamepatch: B: start-up logos %s", PatchLogoWait(script) ? L"can be skipped" : L"can't be skipped - script not as expected");
                }
            }
            if (*reinterpret_cast<uint64_t*>(At(0x1405b1790)) && CancelHeld() && !*At(0x14050e123))
            {
                *At(0x14050e123) = 1;
                Log(L"gamepatch: B: movie stopped");
            }
            int32_t state = *reinterpret_cast<volatile int32_t*>(At(0x1404bc950));
            bool loading = state >= 1 && state <= 5, inLobby = current == kLobbyMenu;
            ULONGLONG since = g_matchLoadingSince, now = GetTickCount64();
            if (!loading && !(inLobby && since))
            {
                g_matchLoadingSince = 0;
                return;
            }
            if (!since) { g_matchLoadingSince = now; return; }
            if (now - since < kHungLoadMs || AiBattle()) return;
            static ULONGLONG toldFor = 0;
            if (toldFor != since)
            {
                toldFor = since;
                Log(L"gamepatch: B: the load into the match has taken %llu s (menu %d, game state %d) - B leaves it", (now - since) / 1000, current, state);
                if (state == 0) SetMenuHeading("STUCK LOADING? PRESS B TO LEAVE");
            }
            if (!CancelHeld() || !g_pushMenu) return;
            g_matchLoadingSince = 0;
            Log(L"gamepatch: B: leaving the hung load (menu %d, game state %d)", current, state);
            if (loading)
            {
                reinterpret_cast<int64_t (*)()>(At(0x140211940))(); // Give Up
                return;
            }
            *At(0x1404bbc34) = 0;
            g_pushMenu(manager, kLobbyLostMenu, 0);
            *reinterpret_cast<char*>(manager + 0x1062c) = 0; // no picked label on the new menu (see OpenCaseMenu)
        }

        void PreviewTick(); // the skill preview outside the town (below)

        using MenuFrameFn = void (*)(int64_t manager);
        MenuFrameFn g_menuFrame = nullptr;
        void PreviewWatchdog(); // where the game stops when the preview hangs (below)
        std::atomic<ULONGLONG> g_menuFrameAt{ 0 };
        std::atomic<DWORD> g_menuFrameThread{ 0 };

        // "Player 2, edits made to the copied arsenal data..." and then "no arsenals" in the lobby's Arsenal Lab
        // (2026-11-14, the developer PC, after a match, with others in the lobby). Read live: menu variable 2 was 0x200
        // again with the Lab open, though OpenLobbyArsenalEditor sets it to 0 - something wrote it back before the
        // Lab's script read it (writers: FUN_140167580, the pause object's mode switch; FUN_1401aefe0, entering a town /
        // stage area; FUN_140216560; the menu scripts). Experimental 167's guess, g5 #112, was never called with it.
        // So on every menu frame of the lobby's Lab, before the scripts run, variable 2 is 0; back in the lobby
        // RestoreLobbyVariable2 gives the lobby its own value as before.
        void KeepLobbyLabVariable2(int64_t manager)
        {
            static bool told = false;
            // The Lab, View Stock and the editor only: the case screens (30 / 31) read variable 2 for the shop's mode.
            int menu = CurrentMenu(manager);
            if (!g_editorFromLobby || (menu != kLabMenu && menu != kStockMenu && menu != kEditorMenu)) { told = false; return; }
            int32_t& v = MenuVariables()[2];
            if (v == 0) return;
            if (!told) Log(L"menu: menu variable 2 became 0x%X in the lobby's Arsenal Lab (menu %d) - 0 again (not split-screen Player 2)", v, CurrentMenu(manager));
            told = true;
            v = 0;
        }

        void HookedMenuFrame(int64_t manager)
        {
            g_menuFrameAt = GetTickCount64();
            if (!g_menuFrameThread) { g_menuFrameThread = GetCurrentThreadId(); PreviewWatchdog(); }
            KeepLobbyLabVariable2(manager);
            g_menuFrame(manager);
            BackButtonTick(manager);
            PreviewTick();
        }

        // The menu manager's per-frame update, FUN_140248230: its vtable slot. Scripts have run when it returns.
        void InstallBackButton()
        {
            static std::once_flag once;
            std::call_once(once, []
            {
                if (!WrapCommand<SessionCommandFn>(0x1403f1150 + 154 * 8, 0x140214d60, &HookedOptionChanged, g_optionChanged, L"settings rows and logo skip")) return;
                uint8_t* slot = At(0x14048e358);
                if (*reinterpret_cast<uint8_t**>(slot) != At(0x140248230))
                {
                    Log(L"gamepatch: B button skipped - the game code is not what this pdnet expects");
                    return;
                }
                std::vector<uint8_t> hook;
                Put64(hook, reinterpret_cast<uint64_t>(&HookedMenuFrame));
                g_menuFrame = reinterpret_cast<MenuFrameFn>(At(0x140248230));
                if (!Write(slot, hook)) { g_menuFrame = nullptr; Log(L"gamepatch: B button failed (VirtualProtect error %lu)", GetLastError()); }
            });
        }

        // pdlaunch keeps the game at splash_volume while the title screen or intro is on (menu 7 playdatasel, 80, or no
        // menu yet): LocalState\pdnet\past-title.txt exists while any other menu is (normally the main menu, 8), and is
        // deleted again when the player backs out to the title. pdlaunch deletes it as the game starts. "No menu" is the
        // intro only before the first time past the title: a match has no menu either (menu -1), at normal volume.
        constexpr int kTitleMenu = 7, kTitleAttractMenu = 80;
        void SignalPastTitle()
        {
            std::thread([]
            {
                std::wstring path = LocalStatePath() + L"\\pdnet\\past-title.txt";
                bool past = false, everPast = false;
                for (;;)
                {
                    Sleep(100);
                    int menu = CurrentMenu(MenuManager());
                    bool now = !(menu == kTitleMenu || menu == kTitleAttractMenu || (menu < 2 && !everPast));
                    everPast = everPast || now;
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
                        pdpl::TitleReached(); // the mod loader's safe mode: this start got past the title screen
                    }
                    else DeleteFileW(path.c_str());
                    Log(L"gamepatch: %s (menu %d): start-up volume %s", past ? L"past the title screen" : L"back on the title screen", menu, past ? L"ends" : L"again");
                }
            }).detach();
        }

        // "Game Rule: <rule>" in a session info box -> "<rule> (Rated)" / "(Unrated)": in the Lobby Browser for the
        // selected search result, in the lobby (its SESSION INFO box) for the lobby this player is in.
        bool RatedRuleText(int64_t* args, char* out, size_t size, bool inLobby)
        {
            char format[64], rule[48];
            if (!CopyString(reinterpret_cast<const char*>(args[2]), format, sizeof(format)) || !strstr(format, "Game Rule")) return false;
            if (!CopyString(reinterpret_cast<const char*>(args[3]), rule, sizeof(rule)) || !rule[0]) return false;
            int rated = -1;
            std::string modTag; // the lobby's gameplay mods (Conceptual, mod loader step 7)
            if (inLobby)
            {
                rated = scores::RatedMatch();
                modTag = mods::LobbyTag(); // joined, so the same as this start's
            }
            else
            {
                int64_t cursor[2] = { 1, 9 }; // the script pushes 9 then 1: the last push is the first argument
                int64_t row = reinterpret_cast<MenuCommandFn>(*reinterpret_cast<void**>(At(0x1403eefe0) + 55 * 8))(cursor);
                rated = row >= 0 ? scores::SearchResultRated(static_cast<size_t>(row)) : -1;
                modTag = row >= 0 ? mods::SearchResultTag(static_cast<size_t>(row)) : std::string();
            }
            // Mods: "(Unrated, 2 mods)"; ", restart to join" when they aren't this start's (joining sets them up for the next).
            const char* restart = !inLobby && mods::Differs(modTag) ? ", restart to join" : "";
            if (!modTag.empty())
            {
                size_t n = mods::Count(modTag);
                sprintf_s(out, size, "%s (Unrated, %zu mod%s%s)", rule, n, n == 1 ? "" : "s", restart);
                return true;
            }
            if (rated < 0 && !*restart) return false;
            if (rated < 0) sprintf_s(out, size, "%s (no mods%s)", rule, restart);
            else sprintf_s(out, size, "%s (%s%s)", rule, rated ? "Rated" : "Unrated", *restart ? ", no mods, restart to join" : "");
            return true;
        }

        // Character select (livecharasel, menu 46): "COLOR \a1f%d" shows a number for each of a character's 4 colours.
        // The user's list (2026-11-11) names them; the number becomes the name. The character is the last name drawn on
        // the screen (the texts drawn there are logged once each, to check).
        struct CharacterColours { const char* name; const char* colours[4]; };
        constexpr CharacterColours kCharacterColours[] = {
            { "Freia", { "Grey", "Red", "White", "Black" } },
            { "Alpha", { "Green", "Red", "Black", "White" } },
            { "Edgar", { "Black", "White", "Orange", "Blue" } },
            { "Meister", { "Black", "White", "Yellow", "Red" } },
            { "Chunky", { "Black", "White", "Yellow", "Red" } },
            { "Cuff Button", { "Black", "White", "Yellow", "Red" } },
            { "PH", { "Black", "White", "Yellow", "Red" } },
            { "Know", { "Green", "Red", "White", "Brown" } },
            { "Tsubutaki", { "Blue", "Yellow", "Orange", "Teal" } },
            { "JD", { "Purple", "Green", "White", "Black" } },
            { "Sammah", { "Yellow", "Blue", "Red", "White" } },
            { "Vestio", { "Brown", "Black", "Purple", "White" } },
            { "Ommato", { "Grey", "Blue", "Red", "Black" } },
            { "Anthro", { "Brown", "White", "Red", "Yellow" } },
            { "Guard", { "Grey", "Full Blue", "Yellow", "Blue" } },
            { "Andro", { "Grey", "Red", "Yellow", "Blue" } },
            { "Ceno", { "White", "Brown", "Purple", "Black" } },
            { "Partheno", { "Grey", "White", "Red", "Yellow" } },
            { "Catoptro", { "Brown", "White", "Red", "Blue" } },
            { "Gyne", { "Maroon", "Yellow", "Red", "White" } },
            { "Claustro", { "Navy", "White", "Black", "Yellow" } },
            { "Scoto", { "Navy", "Red", "White", "Yellow" } },
        };
        const CharacterColours* g_shownCharacter = nullptr;
        int g_gameNameX = 0, g_gameNameY = 0; // where the game draws the name line (command 23), for the Advanced list's plate
        uint8_t g_nameTextColours[16]; bool g_haveNameColours = false; // 0x14054c9b0 (command 27's text colours) at the name line
        int g_gamePlateY = 0; uint32_t g_gamePlateColour = 0; bool g_haveGamePlate = false; // the game's plate sprite 71 (command 95)
        std::string g_charselName; // the name line's text as drawn ("player" for Alpha), for the portrait (HookedDrawSprite)
        // MNet.alr's portraits (MakeMnetCopy, CharacterSelectSprite).
        constexpr int kMnetGameSprites = 84;
        constexpr int kPortraitFirst = 38, kPortraitLast = 48, kMonsterPortraitFirst = 84;
        std::atomic<int> g_monsterPortraits{ 0 }; // sprites added (0: the game's file)
        constexpr int kSpriteSize = 0x3c;
        std::vector<uint8_t> g_mnetSprites; // the copy's sprite records (MakeMnetCopy), to find the game's loaded table
        // The monsters with a portrait, in charsel-portraits.bin's order (sprites 84 on).
        constexpr const char* kMonsterPortraitNames[] = { "Vestio", "Ommato", "Anthro", "Guard", "Andro", "Ceno", "Partheno",
                                                         "Catoptro", "Gyne", "Claustro", "Scoto" };

        // The text with the game's \a control codes (\a + digits + one letter) left out.
        std::string PlainText(const char* text)
        {
            std::string plain;
            for (const char* p = text; *p; ++p)
            {
                if (*p == '\a')
                {
                    while (p[1] >= '0' && p[1] <= '9') ++p;
                    if (p[1]) ++p;
                    continue;
                }
                plain += *p;
            }
            return plain;
        }

        bool CharacterSelectAdvanced(); // (below)
        const char* AdvancedName(const std::string& shown);

        bool ColourNameText(int64_t* args, int64_t* changed)
        {
            if (!args || CurrentMenu(MenuManager()) != kCharacterMenu) return false;
            char format[64];
            if (!CopyString(reinterpret_cast<const char*>(args[2]), format, sizeof(format))) return false;
            std::string text = format;
            std::string value; // the %s argument (Experimental 148 only read it when %s ended the text, and saw no names)
            if (const char* percent = strstr(format, "%s"))
            {
                char copied[48];
                if (args[3] > 0x10000 && CopyString(reinterpret_cast<const char*>(args[3]), copied, sizeof(copied)))
                {
                    value = copied;
                    text = std::string(format, percent - format) + value + (percent + 2);
                }
            }
            static std::set<std::string> logged;
            if (logged.size() < 80 && logged.insert(text).second)
            {
                std::string raw; // the text as the script has it, control codes shown as \a
                for (const char* p = format; *p; ++p) raw += *p == '\a' ? std::string("\\a") : std::string(1, *p);
                Log(L"gamepatch: character select draws '%S' (text '%S', argument %llX)", PlainText(text.c_str()).c_str(), raw.c_str(), args[3]);
            }
            const char* number = strstr(format, "COLOR ");
            if (!number || !strstr(number, "%d"))
            {
                // The name line is "\a130c%s" with the character's name (Experimental 149, MojoJoey's log); the first
                // character, Alpha, is drawn as "player". Any other name there is a character without a list entry.
                bool nameLine = strncmp(format, "\a130c%s", 7) == 0;
                if (nameLine) // "\a130c%s\f" on the character select (the log shows the \f)
                {
                    // Diagnostics (151: Alpha still showed the previous character's colours): every change of the name.
                    static std::string lastName;
                    static int changes = 0;
                    if (value != lastName && changes < 300)
                    {
                        ++changes;
                        Log(L"gamepatch: character select: name line now '%S' (argument %llX)", value.c_str(), args[3]);
                        lastName = value;
                    }
                    g_shownCharacter = nullptr;
                    g_charselName = value;
                    g_gameNameX = static_cast<int>(args[0]), g_gameNameY = static_cast<int>(args[1]);
                    memcpy(g_nameTextColours, At(0x14054c9b0), sizeof(g_nameTextColours)); // the name's yellow (test 8)
                    g_haveNameColours = true;
                    if (_stricmp(value.c_str(), "player") == 0) value = "Alpha";
                }
                for (std::string candidate : { PlainText(text.c_str()), PlainText(value.c_str()) })
                {
                    while (!candidate.empty() && candidate.back() == ' ') candidate.pop_back();
                    while (!candidate.empty() && candidate.front() == ' ') candidate.erase(0, 1);
                    for (const auto& character : kCharacterColours)
                        if (!candidate.empty() && _stricmp(candidate.c_str(), character.name) == 0) g_shownCharacter = &character;
                }
                if (nameLine && CharacterSelectAdvanced())
                {
                    // The Advanced list: the place's entry instead (empty places: no name); its colours are Freia's.
                    static char advancedName[48];
                    strcpy_s(advancedName, AdvancedName(g_charselName));
                    g_shownCharacter = advancedName[0] ? &kCharacterColours[0] : nullptr;
                    memcpy(changed, args, 8 * sizeof(int64_t));
                    changed[3] = reinterpret_cast<int64_t>(advancedName);
                    return true;
                }
                return false;
            }
            if (g_forceConfirm == kConfirmWaitColours) g_forceConfirm = kConfirmColours; // its COLORS window is up: the second A
            int colour = static_cast<int>(args[3]);
            static const CharacterColours* lastUsed = reinterpret_cast<const CharacterColours*>(1);
            static int uses = 0;
            if (g_shownCharacter != lastUsed && uses < 300)
            {
                ++uses;
                Log(L"gamepatch: character select: COLORS rows for %S", g_shownCharacter ? g_shownCharacter->name : "(no character)");
                lastUsed = g_shownCharacter;
            }
            if (!g_shownCharacter || colour < 1 || colour > 4)
            {
                static int told = 0;
                if (told++ < 5) Log(L"gamepatch: character select: COLOR %d left as a number (%s)", colour, g_shownCharacter ? L"not 1-4" : L"no character name seen yet");
                return false;
            }
            // Only the name, in capitals: "COLOR Orange" ran past the COLORS box, and the font's lowercase letters looked
            // out of place (the user, 2026-11-11). "COLOR " goes; the number's own style code (\a1f) stays.
            static char replaced[64], names[4][24]; // a buffer per row: the game may keep the pointer until it draws
            char* name = names[colour - 1];
            std::string withName(number + 6);
            withName.replace(withName.find("%d"), 2, "%s");
            strcpy_s(replaced, (std::string(format, number - format) + withName).c_str());
            strcpy_s(name, sizeof(names[0]), g_shownCharacter->colours[colour - 1]);
            _strupr_s(name, sizeof(names[0]));
            memcpy(changed, args, 8 * sizeof(int64_t));
            changed[2] = reinterpret_cast<int64_t>(replaced);
            changed[3] = reinterpret_cast<int64_t>(name);
            return true;
        }

        int64_t HookedDrawText(int64_t* args)
        {
            if (int64_t row[8]; CaseRowText(args, row)) return g_drawText(row);
            if (int64_t named[8]; ColourNameText(args, named)) return g_drawText(named);
            int menu = CurrentMenu(MenuManager());
            if (args && GetConfig().ratedLobbies && (menu == kSessionListMenu || menu == kLobbyMenu))
            {
                static char text[96];
                if (RatedRuleText(args, text, sizeof(text), menu == kLobbyMenu))
                {
                    int64_t changed[8];
                    memcpy(changed, args, sizeof(changed));
                    changed[3] = reinterpret_cast<int64_t>(text);
                    return g_drawText(changed);
                }
                if (menu == kSessionListMenu) return g_drawText(args); // the lobby's other texts go on below (talking icons)
            }
            char format[24];
            if (args && CopyString(reinterpret_cast<const char*>(args[2]), format, sizeof(format)) && strncmp(format, "\a580rSkill Update", 17) == 0)
            {
                // Both lines kRaise px higher than the game's own spot: the version line under it ran into the main
                // menu's hint line (the item descriptions, left of it on the same row; 2026-10-29).
                constexpr int64_t kRaise = 24;
                int64_t skill[8];
                memcpy(skill, args, sizeof(skill));
                skill[1] -= kRaise;
                int64_t result = g_drawText(skill);
                static std::string line = std::string("\a580r") + BuildLabel();
                int64_t label[8] = { args[0], skill[1] + 20, reinterpret_cast<int64_t>(line.c_str()) };
                g_drawText(label);
                return result;
            }
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

        int64_t CharacterSelectSprite(int64_t* args); // (below)
        void CharacterSelectLeft();

        int LobbyBanner(int sprite); // (below, with the variants)
        void LobbyBannerHandles();

        int64_t HookedDrawSprite(int64_t* args)
        {
            if (args && CurrentMenu(MenuManager()) == kCharacterMenu) return CharacterSelectSprite(args);
            CharacterSelectLeft();
            if (args && CurrentMenu(MenuManager()) == kLobbyMenu)
            {
                // Test 9 (2026-11-21): no window_chara draw came through here, so log what the lobby draws (once each), and
                // also swap the carriers' handles in MNET's list (any draw path that reads the list shows the banner).
                static std::set<std::pair<int64_t, int64_t>> seen;
                if (seen.size() < 40 && seen.emplace(args[0], args[1]).second)
                    Log(L"variants: lobby draws sheet %lld sprite %lld at %lld,%lld", args[0], args[1], args[2], args[3]);
                LobbyBannerHandles();
                if (args[0] == 3 && args[1] >= 0 && args[1] <= 26)
                    if (int banner = LobbyBanner(static_cast<int>(args[1])); banner >= 0)
                    {
                        int64_t shown[8];
                        memcpy(shown, args, sizeof(shown));
                        shown[1] = banner;
                        return g_drawSprite(shown);
                    }
            }
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

        // The character select's portraits (see MakeMnetCopy, ShowGrid): while the name line (ColourNameText) shows a
        // monster, the grid shows each place's monster (sprites 84 on, kMonsterFor). Every distinct sprite drawn there is
        // logged once, to check.
        // The Advanced list (2026-11-20, the user's request): Y (controller) or the Y key on the character select switches
        // g_charselAdvanced: heading ADVANCED, the button bar's Y entry (\a5# is the Y icon: "\a5#Invite" in the lobby),
        // and the test list: Freia (jump) / Freia (dodge) in two grid places (ShowGrid, AdvancedName). Still to come: the
        // 12 colour rows "<colour> - <jump>", and the pick itself (the variant characters don't exist yet).
        std::atomic<bool> g_charselAdvanced{ false };
        enum PopupStep { kNoPopup, kColourStep, kDodgeStep, kSetStep }; // the Advanced list's pop-ups (CharacterSelectButtons)
        std::atomic<int> g_popup{ kNoPopup };
        std::atomic<bool> g_charselKeyY{ false }; // the Y key went down (HookedKey: the game's window delivers keys)
        constexpr uint32_t kPadY = 0x20; // Windows.Gaming.Input.GamepadButtons.Y (A 4, B 8, X 0x10, Y 0x20; 0x80 is D-pad down)

        // The screen shows all 11 portraits at once, a grid of sprites 38-48 (player_00..10: player, Edgar, Freia,
        // Meister, Chunky, Cuff Button, pH, Know, Tsubutaki, JD, Sammah); with Toggle Monsters on it draws the same
        // sprites. Each grid place has its monster, as in the user's guide images (Freia -> Vestio, player -> Ommato, as
        // the logs show): kMonsterFor[sprite - 38] = index in kMonsterPortraitNames.
        constexpr int kMonsterFor[11] = { 1 /* player: Ommato */, 2 /* Edgar: Anthro */, 0 /* Freia: Vestio */, 3 /* Meister: Guard */,
                                          4 /* Chunky: Andro */, 5 /* Cuff Button: Ceno */, 6 /* pH: Partheno */, 7 /* Know: Catoptro */,
                                          8 /* Tsubutaki: Gyne */, 9 /* JD: Claustro */, 10 /* Sammah: Scoto */ };

        // True while the name line shows a monster (Toggle Monsters on).
        bool ShowingMonsters()
        {
            for (const char* name : kMonsterPortraitNames)
                if (_stricmp(g_charselName.c_str(), name) == 0) return true;
            for (const char* name : { "Mechano", "Euroto", "Hedono", "Germano", "Belono" })
                if (_stricmp(g_charselName.c_str(), name) == 0) return true;
            return false;
        }

        // The heading the screen had: Test Arsenal's (HookedAddItem), else the multiplayer menus' "Online".
        const char* CharacterSelectHeading()
        {
            return g_testPicking == kPickingOpponentCharacter ? "AI OPPONENT'S CHARACTER" : g_testPicking == kPickingCharacter ? "YOUR CHARACTER" : "Online";
        }

        const char* AdvancedHeading(); // (below, with the pages)

        void CharacterSelectInput()
        {
            static bool held = false;
            static ULONGLONG lastFrame = 0;
            ULONGLONG now = GetTickCount64();
            if (now - lastFrame > 1000) // the screen just opened: the normal list
            {
                if (g_charselAdvanced.exchange(false))
                {
                    SetMenuHeading(CharacterSelectHeading());
                    Log(L"gamepatch: character select: normal list (screen opened)");
                }
                held = true; // a Y still held from the last screen doesn't count
            }
            lastFrame = now;
            uint8_t keys[256];
            bool pad = AnyPadHolds(kPadY), key = g_charselKeyY.exchange(false) || (KeysHeld(keys) && keys['Y']);
            bool down = pad || key;
            static ULONGLONG nextBeat = 0; // diagnostics while the screen is open: every 2 s, up to 60 lines
            static int beats = 0;
            if (now >= nextBeat && beats < 60)
            {
                ++beats;
                nextBeat = now + 2000;
                Log(L"gamepatch: character select: name '%S', monsters %d, Y pad %d key %d, Advanced %d", g_charselName.c_str(), ShowingMonsters() ? 1 : 0,
                    pad ? 1 : 0, key ? 1 : 0, g_charselAdvanced ? 1 : 0);
            }
            if (down && !held && g_popup == kNoPopup)
            {
                bool advanced = !g_charselAdvanced;
                g_charselAdvanced = advanced;
                Log(L"gamepatch: character select: %s list (%s)", advanced ? L"Advanced" : L"normal", pad ? L"pad Y" : L"Y key");
                g_popup = kNoPopup;
                SetMenuHeading(advanced ? AdvancedHeading() : CharacterSelectHeading());
            }
            held = down;
        }

        bool SameBytes(const uint8_t* at, const uint8_t* want, size_t size)
        {
            __try { return memcmp(at, want, size) == 0; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        // How the game draws a sheet's sprite (menu command 95 = FUN_140246c70): the menu manager keeps per texture slot
        // (manager + 0x10530 + slot * 32) a pointer at +0x18 to a u32 per sprite, the sprite's handle for the drawing
        // code (FUN_1401983e0). Test 1 swapped the sprite number in command 95: only the back layer changed. Test 2 / 3
        // switched sprite records in memory: no effect (and test 3's search of all memory took 2.5 s per frame).
        // Now the handles of sprites 38-48 in MNET's (slot 3) list are switched: every drawing that goes through that
        // list shows the monster / Advanced picture. The game's own handles go back when the screen is left.
        constexpr int kBlankSprite = kMonsterPortraitFirst + 11; // blank.tga: transparent, after the monsters
        uint32_t g_ownHandles[11], g_putHandles[11]; // the game's, and the last ones pdnet put there
        uint32_t* g_handles = nullptr; // the list the own handles were read from
        // the game's cursor frames (66-69), name plate (70-72): hidden on the Advanced list (test 4: the white card and red
        // bar of 68 / 69 / 70 still followed the game's hidden cursor). [0] is player_cursor2, pdnet's own cursor frame.
        constexpr int kFrameSprites[7] = { 67, 66, 68, 69, 70, 71, 72 };
        constexpr int kFrameCount = 7;
        uint32_t g_ownFrames[kFrameCount];
        int g_gridShown = -1; // what the grid shows: 0 own, 1 monsters, 2 Advanced
        ULONGLONG g_gridLastFrame = 0;

        uint32_t* MnetHandles()
        {
            int64_t manager = MenuManager();
            if (!manager) return nullptr;
            uint8_t* slot = reinterpret_cast<uint8_t*>(manager) + 0x10530 + kMultiplayerTextures * 32;
            uint64_t loaded = 0, list = 0;
            if (!SameBytes(slot, slot, 32)) return nullptr;
            memcpy(&loaded, slot, 8);
            memcpy(&list, slot + 0x18, 8);
            return loaded && list ? reinterpret_cast<uint32_t*>(list) : nullptr;
        }

        // The Advanced list (the user's design, 2026-11-20; docs/CHARACTER_SELECT.md): all of variants::Characters() at once,
        // a grid of 6 x 5 portraits drawn by pdnet at 55 % size (DrawAdvancedGrid), with its own cursor (CharacterSelectButtons:
        // D-pad, stick, arrow keys, W A S D). The game's own grid stays underneath but shows nothing: its sprites 38-48 and
        // its cursor frame / name plate (67, 71, 72) get the blank sprite's handle (ShowGrid), and their command 95 draws
        // are skipped. Each entry has a sprite of its own (one object on screen per sprite): players their player_NN,
        // the others portrait_<name> (96-110) and portrait_<name>2 (111-114) for the second models.
        // Test 2: no gaps between portraits, as big as fits above the game's "Choose the character" line.
        // Test 3: off the top bar, room under the bottom row for the name: 64 % = 85 x 61 per entry, 510 x 305 from y 50.
        constexpr float kGridScale = 0.64f;
        constexpr int kGridColumns = 6, kGridRows = 5, kGridPitchX = static_cast<int>(134 * kGridScale), kGridPitchY = static_cast<int>(96 * kGridScale);
        constexpr int kGridX = (640 - kGridColumns * kGridPitchX) / 2, kGridY = 58; // test 5: 8 px lower
        constexpr int kFirstFrameSprite = 66, kNamePlateRight = 72;
        std::atomic<int> g_gridCursor{ 0 }; // index in variants::Characters() (all pages)
        // Pages (2026-11-21, for characters added with variants::AddCharacter): 30 entries a page, LB / RB or Q / E turn
        // pages, the heading shows "ADVANCED n/m" when there is more than one.
        constexpr int kPageSize = kGridColumns * kGridRows;
        int PageCount() { return (std::max)(1, (static_cast<int>(variants::Characters().size()) + kPageSize - 1) / kPageSize); }
        const char* AdvancedHeading()
        {
            static char text[32];
            if (PageCount() <= 1) return "ADVANCED";
            sprintf_s(text, "ADVANCED %d/%d", g_gridCursor / kPageSize + 1, PageCount());
            return text;
        }

        // The grid place (sprite 38-48) of the name the game draws there (a player's or, with Toggle Monsters, a monster's).
        int PlaceOfName(const std::string& shown)
        {
            static const char* kPlayers[11] = { "player", "Edgar", "Freia", "Meister", "Chunky", "Cuff Button", "pH", "Know", "Tsubutaki", "JD", "Sammah" };
            for (int k = 0; k < 11; ++k)
            {
                if (_stricmp(shown.c_str(), kPlayers[k]) == 0) return kPortraitFirst + k;
                if (_stricmp(shown.c_str(), kMonsterPortraitNames[kMonsterFor[k]]) == 0) return kPortraitFirst + k;
            }
            return -1;
        }

        // An entry's portrait sprite: a player's own (player_00..10 in the game's character order), else portrait_<name>.
        int EntrySprite(int entry)
        {
            static const char* kPlayers[11] = { "player", "Edgar", "Freia", "Meister", "Chunky", "Cuff Button", "pH", "Know", "Tsubutaki", "JD", "Sammah" };
            // charsel-portraits.bin's EXTRA (96 on) and DUPLICATES (111 on) orders, tools\charsel\make_portraits.py
            static const char* kExtra[15] = { "Germano", "Belono", "03", "Spokesman", "Ubiquitous", "Kajikawa", "Tetsuya", "Arthur",
                                              "Reindeer", "Mac", "Baroness", "Mikan", "Ai", "Maniac", "Kei" };
            static const char* kSecond[1] = { "Ubiquitous" }; // DUPLICATES (Mac / Baroness / Reindeer 2 are gone)
            if (entry < 0 || entry >= static_cast<int>(variants::Characters().size())) return kBlankSprite;
            const variants::Character& c = variants::Characters()[entry];
            if (c.portrait >= 0 && c.portrait < kMonsterPortraitFirst + g_monsterPortraits) return c.portrait; // an added character's own
            std::string name = c.name;
            for (int k = 0; k < 11; ++k) if (_stricmp(name.c_str(), kPlayers[k]) == 0) return kPortraitFirst + k;
            bool second = c.version == 1; // a second model: portrait_<name>2
            if (c.kind == variants::Kind::Npc) name = name.substr(0, name.find(' ')); // "Ubiquitous Keyboard" -> its portrait's name
            int last = kMonsterPortraitFirst + g_monsterPortraits - 1;
            if (second)
                for (int k = 0; k < 1; ++k)
                    if (_stricmp(name.c_str(), kSecond[k]) == 0 && kBlankSprite + 16 + k <= last) return kBlankSprite + 16 + k;
            for (int k = 0; k < 15; ++k)
                if (_stricmp(name.c_str(), kExtra[k]) == 0 && kBlankSprite + 1 + k <= last) return kBlankSprite + 1 + k;
            return kPortraitFirst; // placeholder: the player's portrait
        }

        // The game's name plate shows nothing on the Advanced list (pdnet writes the name under its own grid).
        const char* AdvancedName(const std::string&) { return ""; }

        bool CharacterSelectAdvanced() { return g_charselAdvanced; }


        // The sprite a grid place (sprite 38-48) shows now.
        int ShownAt(int place, int what)
        {
            if (what == 2) return kBlankSprite; // the Advanced list draws its own grid
            int monster = kMonsterFor[place - kPortraitFirst];
            return what == 1 && monster < g_monsterPortraits ? kMonsterPortraitFirst + monster : place;
        }

        bool PutHandles(uint32_t* list, const uint32_t* handles)
        {
            __try { memcpy(list + kPortraitFirst, handles, sizeof(g_ownHandles)); return true; }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        void ShowGrid(int what)
        {
            uint32_t* list = MnetHandles();
            if (!list || g_monsterPortraits <= 11) return;
            // A new load of MNET (another list, or the same memory with other handles than pdnet's last): its own handles.
            bool reloaded = list != g_handles || !SameBytes(reinterpret_cast<uint8_t*>(list + kPortraitFirst), reinterpret_cast<uint8_t*>(g_putHandles), sizeof(g_putHandles));
            if (!reloaded && what == g_gridShown) return;
            if (reloaded)
            {
                if (!SameBytes(reinterpret_cast<uint8_t*>(list + kPortraitFirst), reinterpret_cast<uint8_t*>(list + kPortraitFirst), sizeof(g_ownHandles))) return;
                memcpy(g_ownHandles, list + kPortraitFirst, sizeof(g_ownHandles));
                for (int i = 0; i < kFrameCount; ++i) g_ownFrames[i] = list[kFrameSprites[i]];
                g_handles = list;
                static int told = 0;
                if (told++ < 10) Log(L"gamepatch: character select: MNET sprite handles 38-48 %u..%u, monsters from %u", g_ownHandles[0], g_ownHandles[10], list[kMonsterPortraitFirst]);
            }
            uint32_t handles[11];
            for (int k = 0; k < 11; ++k)
            {
                int shown = ShownAt(kPortraitFirst + k, what);
                handles[k] = shown >= kPortraitFirst && shown <= kPortraitLast ? g_ownHandles[shown - kPortraitFirst] : list[shown];
            }
            if (!PutHandles(list, handles)) return;
            g_gridShown = what;
            memcpy(g_putHandles, handles, sizeof(handles));
            // the game's cursor frame and name plate: hidden on the Advanced list (pdnet draws its own cursor)
            for (int i = 0; i < kFrameCount; ++i) list[kFrameSprites[i]] = what == 2 ? list[kBlankSprite] : g_ownFrames[i];
        }

        // Draws a sprite of MNET by its handle (FUN_1401983e0: the object's position, size and colour for this frame;
        // its 6th / 7th arguments point to x / y scales, the 8th to the colour). The game's grid places are drawn this way
        // by FUN_140246c70 (menu command 95) with no scale.
        using SpriteObjectFn = void (*)(void* unused, uint32_t handle, int x, int y, int64_t a5, const float* scaleX, const float* scaleY,
                                        const uint32_t* colour, int64_t a9, int64_t a10);
        void DrawHandle(uint32_t handle, int x, int y, float scale, uint32_t colour)
        {
            reinterpret_cast<SpriteObjectFn>(At(0x1401983e0))(nullptr, handle, x, y, 0, &scale, &scale, &colour, 0, 0);
        }

        uint32_t OwnHandle(uint32_t* list, int sprite)
        {
            if (sprite >= kPortraitFirst && sprite <= kPortraitLast && list == g_handles) return g_ownHandles[sprite - kPortraitFirst];
            return list[sprite];
        }

        void DrawAdvancedGrid()
        {
            uint32_t* list = MnetHandles();
            if (!list) return;
            int count = static_cast<int>(variants::Characters().size()), cursor = g_gridCursor;
            int first = cursor / kPageSize * kPageSize; // this page's entries
            for (int e = first; e < count && e < first + kPageSize; ++e)
            {
                int x = kGridX + ((e - first) % kGridColumns) * kGridPitchX, y = kGridY + ((e - first) / kGridColumns) * kGridPitchY;
                DrawHandle(OwnHandle(list, EntrySprite(e)), x, y, kGridScale, e == cursor ? 0xffffffff : 0xff9a9a9a);
            }
            cursor -= first;
            // the game's own cursor frame (player_cursor2, 138 x 100 around a 134 x 96 portrait), on the chosen entry
            uint32_t frame = list == g_handles ? g_ownFrames[0] : 0;
            if (frame)
                DrawHandle(frame, kGridX + (cursor % kGridColumns) * kGridPitchX - 1, kGridY + (cursor / kGridColumns) * kGridPitchY - 1, kGridScale, 0xffffffff);
        }

        int64_t CharacterSelectSprite(int64_t* args)
        {
            g_gridLastFrame = GetTickCount64();
            CharacterSelectInput(); // once per drawn sprite: several times a frame, edges only
            ShowGrid(g_charselAdvanced ? 2 : ShowingMonsters() ? 1 : 0);
            static std::set<std::pair<int64_t, int64_t>> seen;
            if (seen.size() < 60 && seen.emplace(args[0], args[1]).second)
                Log(L"gamepatch: character select draws sheet %lld sprite %lld at %lld,%lld (name line '%S')", args[0], args[1], args[2], args[3], g_charselName.c_str());
            if (args[0] == 3 && args[1] == kNamePlateRight - 1) // 71: the game's plate, its y and colour for pdnet's (test 8)
            {
                g_gamePlateY = static_cast<int>(args[3]), g_gamePlateColour = static_cast<uint32_t>(args[4]), g_haveGamePlate = true;
                static int told = 0;
                if (told++ < 2) Log(L"gamepatch: character select: plate sprite at %lld,%lld colour %llX, name line at %d,%d", args[2], args[3], args[4], g_gameNameX, g_gameNameY);
            }
            if (g_charselAdvanced && args[0] == 3 &&
                ((args[1] >= kPortraitFirst && args[1] <= kPortraitLast) || (args[1] >= kFirstFrameSprite && args[1] <= kNamePlateRight)))
                return 1; // pdnet's grid is drawn after the screen's window (DrawCharselPopup)
            return g_drawSprite(args);
        }

        // Leaving the screen: the game's own handles back (the lobby draws the same sheet), if MNET is still that load.
        void CharacterSelectLeft()
        {
            if (g_gridShown <= 0 || GetTickCount64() - g_gridLastFrame < 300) return;
            if (MnetHandles() == g_handles && g_handles)
            {
                PutHandles(g_handles, g_ownHandles);
                for (int i = 0; i < kFrameCount; ++i) g_handles[kFrameSprites[i]] = g_ownFrames[i];
            }
            g_gridShown = 0;
        }

        // The button bar: "\a6#Select \a580r\a4#Toggle Monsters \a2#Confirm \a3#Back" ($MONSTER_HINT) gets the Y entry.
        bool CharacterSelectBar(const char* text, char* out, size_t size)
        {
            if (CurrentMenu(MenuManager()) != kCharacterMenu) return false;
            static std::set<std::string> told;
            if (told.size() < 10 && told.insert(text).second)
            {
                std::string shown;
                for (const char* p = text; *p; ++p) shown += *p == '\a' ? std::string("\\a") : std::string(1, *p);
                Log(L"gamepatch: character select: button bar '%S'", shown.c_str());
            }
            if (strcmp(text, "$MONSTER_HINT") == 0) text = "\a6#Select \a580r\a4#Toggle Monsters \a2#Confirm \a3#Back"; // newstrings.txt monster_hint (English)
            const char* toggle = strstr(text, "\a4#Toggle Monsters ");
            if (!toggle) return false;
            // Normal: "... X Toggle Monsters  Y Advanced  A Confirm  B Back". Advanced: X does nothing there, so its entry goes:
            // "... Y All Characters  A Confirm  B Back" (the name of the list Y goes back to).
            size_t before = toggle - text, after = before + strlen("\a4#Toggle Monsters ");
            if (g_charselAdvanced) _snprintf_s(out, size, _TRUNCATE, "%.*s\a5#All Characters %s", static_cast<int>(before), text, text + after);
            else _snprintf_s(out, size, _TRUNCATE, "%.*s\a5#Advanced %s", static_cast<int>(after), text, text + after);
            return true;
        }

        // The Advanced list's three menus after a pick (the user, 2026-11-20): Colour, Dodge type, Animation set, one after
        // the other in a pop-up window (window 8, set up like the Settings' save question: menu commands 33 / 36 / 37 /
        // 38 / 40, drawn after the screen's window 12 with command 42, its rows with command 24). While it's open the
        // script gets no buttons (CharacterSelectButtons): up / down (D-pad, stick, arrow keys, W / S) move, A picks, B
        // goes back. The last A hands the pick to the game: pdnet presses A twice for the script (the grid place, then
        // the game's own COLORS row) and the store (g4 #571) gets the variant's carrier value (ActivateVariant).
        std::atomic<int> g_popupRow{ 0 };
        variants::Pick g_popupPick;
        std::atomic<bool> g_variantPending{ false }; // the next pick store is the Advanced list's
        std::atomic<int> g_charselKeyMove{ 0 };        // arrow keys / W A S D from the game's key handler: 1 up, 2 down, 3 left, 4 right
        std::atomic<int> g_charselKeyPage{ 0 };        // Q -1 / E +1: the Advanced list's pages
        constexpr int kCharselPopupWindow = 8, kCharselPopupX = 170, kCharselPopupW = 300;
        constexpr uint32_t kPadUp = 0x40, kPadDown = 0x80, kPadLeft = 0x100, kPadRight = 0x200;

        int PopupRows()
        {
            switch (g_popup.load())
            {
            case kColourStep: return (std::max)(1, variants::ColourCount(variants::Characters()[g_popupPick.character]));
            case kDodgeStep: return static_cast<int>(variants::Dodges().size());
            case kSetStep: return static_cast<int>(variants::AnimationSets().size());
            default: return 0;
            }
        }

        const char* PopupTitle()
        {
            return g_popup == kColourStep ? "Color" : g_popup == kDodgeStep ? "Movement Type" : "Animation Set";
        }

        std::string PopupRowText(int row)
        {
            const variants::Character& c = variants::Characters()[g_popupPick.character];
            if (g_popup == kColourStep) return c.colours[row] ? c.colours[row] : "Color 1";
            if (g_popup == kDodgeStep) return variants::Dodges()[row].name;
            return variants::AnimationSets()[row].name;
        }

        // Layout (test 1: the rows ran one row below the window and its title didn't show): pdnet writes the title itself,
        // rows start one line under it, the window is tall enough for both.
        constexpr int kPopupLine = 22, kPopupTop = 12;
        int PopupHeight() { return kPopupTop * 2 + (PopupRows() + 1) * kPopupLine + 28; } // test 3: more room under the last row
        int PopupY() { return 200 - PopupHeight() / 2; }

        void OpenCharselPopup(int step)
        {
            g_popup = step;
            g_popupRow = 0;
            int64_t manager = MenuManager();
            if (!manager || step == kNoPopup) return;
            int16_t current = *reinterpret_cast<int16_t*>(manager + 0x10402); // the script's own window stays current
            auto cmd = [](uint64_t fn, int64_t a0, int64_t a1) { int64_t args[8] = { a0, a1 }; reinterpret_cast<MenuCommandFn>(At(fn))(args); };
            cmd(0x140207cd0, kCharselPopupWindow, 2);
            cmd(0x140207d20, kCharselPopupW, PopupHeight());
            cmd(0x140207d50, kCharselPopupX, PopupY());
            cmd(0x140207d80, reinterpret_cast<int64_t>(""), 0);
            // empty lines as the Settings' save question (test 4: a window with none drew no background)
            // test 5: the window's height follows its lines; the short pop-ups (Color, Movement Type) get one more
            int lines = PopupRows() + (PopupRows() < 6 ? 2 : 1);
            for (int i = 0; i < lines; ++i) cmd(0x140207e20, reinterpret_cast<int64_t>(""), 0);
            *reinterpret_cast<int16_t*>(manager + 0x10402) = current;
        }

        // After the screen's window (12) is drawn: on the Advanced list the chosen entry's name under pdnet's grid, and the
        // pop-up with its title and rows (the chosen one marked).
        void DrawNamePlate(const char* name, int centreX, int textY); // (below, with the hooks)

        // The chosen entry's name on its plate, under it (test 3), after the grid. Not while a pop-up is open (text is
        // drawn over every window background). Tests 6 / 7: filled rectangles (command 29) lie under every sprite, so
        // the plate is the game's own plate sprites: player_name_plate_l (71, 20 x 16), _c (70, 8 x 16, stretched), _r (72).
        void DrawAdvancedName()
        {
            int64_t manager = MenuManager();
            int cursor = g_gridCursor;
            if (!manager || g_popup != kNoPopup || cursor < 0 || cursor >= static_cast<int>(variants::Characters().size())) return;
            int64_t* window = reinterpret_cast<int64_t*>(manager + 0x10408);
            int64_t previous = *window;
            *window = reinterpret_cast<int64_t>(WindowAt(manager, 12));
            // the plate under the entry; the name where the game has it against its own plate (test 8: 9 px was too low)
            int place = cursor % kPageSize; // on its page
            int x = kGridX + (place % kGridColumns) * kGridPitchX + kGridPitchX / 2, plateY = kGridY + (place / kGridColumns + 1) * kGridPitchY;
            int y = plateY + (g_haveGamePlate ? g_gameNameY - g_gamePlateY : 1);
            DrawNamePlate(variants::Characters()[cursor].name, x, plateY);
            static char name[64];
            sprintf_s(name, "\a200c%s", variants::Characters()[cursor].name);
            int64_t a[8] = { x - 100, y, reinterpret_cast<int64_t>(name) };
            uint8_t colours[16];
            uint8_t* textColours = At(0x14054c9b0);
            memcpy(colours, textColours, sizeof(colours));
            if (g_haveNameColours) memcpy(textColours, g_nameTextColours, sizeof(colours)); // the game's yellow for names
            reinterpret_cast<MenuCommandFn>(At(0x140207870))(a);
            memcpy(textColours, colours, sizeof(colours));
            *window = previous;
        }

        void DrawCharselPopup(int64_t* args)
        {
            int64_t manager = MenuManager();
            if (!g_charselAdvanced || !args || args[0] != 12 || !manager || !g_drawWindow) return;
            // Test 2: drawn with the portraits (command 95 of sprite 40) the grid sometimes lay under the screen's scanlines,
            // sometimes over them. After window 12 it should always be over them.
            DrawAdvancedGrid();
            DrawAdvancedName();
            auto text = [](int x, int y, const char* s) { int64_t a[8] = { x, y, reinterpret_cast<int64_t>(s) }; reinterpret_cast<MenuCommandFn>(At(0x140207870))(a); };
            if (g_popup == kNoPopup) return;
            int64_t popup[8] = { kCharselPopupWindow };
            g_drawWindow(popup);
            *reinterpret_cast<int64_t*>(manager + 0x10408) = reinterpret_cast<int64_t>(WindowAt(manager, kCharselPopupWindow));
            static char lines[9][96];
            int y = PopupY() + kPopupTop;
            sprintf_s(lines[8], "\a%dc%s", kCharselPopupW, PopupTitle());
            text(kCharselPopupX, y, lines[8]);
            y += kPopupLine + 6;
            for (int r = 0; r < PopupRows() && r < 8; ++r, y += kPopupLine)
            {
                std::string row = PopupRowText(r);
                _snprintf_s(lines[r], _TRUNCATE, r == g_popupRow ? "\a%dc> %s <" : "\a%dc%s", kCharselPopupW, row.c_str());
                text(kCharselPopupX, y, lines[r]);
            }
            *reinterpret_cast<int64_t*>(manager + 0x10408) = 0;
        }

        void PopupSound(int which) { int64_t sound[2] = { which, 0 }; reinterpret_cast<InputFn>(At(0x140261750))(sound); }

        // The script's button reads on the character select (HookedButtons). A = 0x1010, B = 0x2020 (as the Settings).
        // On the Advanced list pdnet takes every input: the grid cursor, the pop-ups; the script sees no A / B and the game's
        // window input nothing (HookedWindowInput: its hidden grid cursor stays put). The last pop-up's A stores the pick (g4 #571 with the
        // variant's carrier value, as the script does after its COLORS row) and backs out of the screen (g_forceBack:
        // the B event for the script's next input label, test 1's two A presses for the script never arrived).
        using PickFn = int64_t (*)(int32_t* args);
        PickFn g_storePick = nullptr;
        int ActivateVariant(int slot, const variants::Pick& pick); // (below, with the carriers)
        void ClearVariant(int slot);
        void PrebuildVariant(int slot, const variants::Pick& pick, const char* why);
        void PublishVariant(int slot, const variants::Pick* pick);
        void LogPickTable(const char* why);
        void CarrierClash(int record, int slot);
        void RetryMemberVariants(const char* why);

        int PickSlot()
        {
            if (g_testPicking == kPickingOpponentCharacter) return 1; // Test Arsenal: the AI opponent's pick
            if (g_testPicking == kPickingCharacter) return 0;
            return *reinterpret_cast<int32_t*>(At(0x140527bc4)); // this PC's lobby slot
        }

        void FinishAdvancedPick()
        {
            int slot = PickSlot();
            const variants::Pick& k = g_popupPick;
            const variants::Character& c = variants::Characters()[k.character];
            int value = -1;
            if (c.kind == variants::Kind::Monster)
            {
                // Germano / Belono: the game's own record (no variant, nothing to build or share: every PC has them)
                if (slot >= 0 && slot < 4) ClearVariant(slot);
                value = (k.colour & 0xffff) << 16 | c.record;
                CarrierClash(c.record, slot);
                Log(L"variants: Advanced list: %S colour %d (the game's own record %d) -> slot %d, value %X", c.name, k.colour, c.record, slot, value);
                PublishVariant(slot, nullptr);
            }
            else
            {
                value = slot >= 0 && slot < 4 ? ActivateVariant(slot, k) : -1;
                Log(L"variants: Advanced list: %S, colour %d, %S, %S set -> slot %d, value %X", c.name, k.colour,
                    variants::Dodges()[k.dodge].name, variants::AnimationSets()[k.set].name, slot, value);
                if (value >= 0) PrebuildVariant(slot, k, "this PC's pick");
                PublishVariant(slot, value >= 0 ? &k : nullptr);
            }
            LogPickTable("after the Advanced pick");
            if (value < 0 || !g_storePick) return;
            if (g_testPicking == kPickingNone)
            {
                // a lobby: the game's own confirm (see g_forceConfirm)
                g_pendingPick = value;
                g_forceConfirmSince = GetTickCount64();
                g_forceConfirm = kConfirmGrid;
                Log(L"variants: Advanced list: confirming through the script (value %X waits for its store)", value);
                return;
            }
            int32_t args[16] = { value }; // Test Arsenal: stored here, the screen left with B (works there)
            g_variantPending = false;
            g_storePick(args);
            g_forceBack = true;
        }

        int64_t CharacterSelectButtons(int64_t buttons)
        {
            static bool heldA = false, heldB = false, held[4] = {};
            bool a = (buttons & 0x1010) != 0, b = (buttons & 0x2020) != 0;
            float lx = 0, ly = 0, rx = 0, ry = 0;
            PadSticks(lx, ly, rx, ry);
            bool dir[4] = { AnyPadHolds(kPadUp) || ly > 0.6f, AnyPadHolds(kPadDown) || ly < -0.6f,
                            AnyPadHolds(kPadLeft) || lx < -0.6f, AnyPadHolds(kPadRight) || lx > 0.6f };
            int keyMove = g_charselKeyMove.exchange(0);
            bool press[4];
            for (int d = 0; d < 4; ++d) press[d] = dir[d] && !held[d], held[d] = dir[d];
            if (keyMove) press[keyMove - 1] = true; // 1 up, 2 down, 3 left, 4 right
            static bool heldPage[2] = {};
            bool pageDir[2] = { AnyPadHolds(0x400), AnyPadHolds(0x800) }; // LB, RB
            int keyPage = g_charselKeyPage.exchange(0);                   // Q -1, E +1
            int turn = 0;
            if ((pageDir[0] && !heldPage[0]) || keyPage < 0) turn = -1;
            if ((pageDir[1] && !heldPage[1]) || keyPage > 0) turn = 1;
            heldPage[0] = pageDir[0], heldPage[1] = pageDir[1];
            bool pressA = a && !heldA, pressB = b && !heldB;
            heldA = a, heldB = b;
            if (!g_charselAdvanced) return buttons;

            if (g_popup == kNoPopup)
            {
                int count = static_cast<int>(variants::Characters().size()), cursor = g_gridCursor;
                int page = cursor / kPageSize, pages = PageCount(), place = cursor % kPageSize;
                if (turn && pages > 1) page = (page + pages + turn) % pages; // same place on the other page (or its last entry)
                int onPage = (std::min)(kPageSize, count - page * kPageSize);
                int column = place % kGridColumns, row = place / kGridColumns, rows = (onPage + kGridColumns - 1) / kGridColumns;
                if (row >= rows) row = rows - 1;
                if (press[0]) row = (row + rows - 1) % rows;
                if (press[1]) row = (row + 1) % rows;
                if (press[2]) column = (column + kGridColumns - 1) % kGridColumns;
                if (press[3]) column = (column + 1) % kGridColumns;
                int next = page * kPageSize + (std::min)(row * kGridColumns + column, onPage - 1);
                if (next != cursor)
                {
                    bool newPage = next / kPageSize != cursor / kPageSize;
                    g_gridCursor = next;
                    PopupSound(37);
                    if (newPage) SetMenuHeading(AdvancedHeading()), Log(L"gamepatch: Advanced list: page %d of %d", next / kPageSize + 1, pages);
                }
                if (pressB)
                {
                    // back to the normal list (B again leaves the screen as usual)
                    g_charselAdvanced = false;
                    SetMenuHeading(CharacterSelectHeading());
                    PopupSound(1);
                    Log(L"gamepatch: character select: normal list (B)");
                }
                else if (pressA)
                {
                    g_popupPick = { g_gridCursor, 0, 0, 0 };
                    PopupSound(0);
                    OpenCharselPopup(kColourStep);
                    Log(L"gamepatch: Advanced list: %S chosen", variants::Characters()[g_gridCursor].name);
                }
                return 0; // the script gets nothing (test 2: its hidden cursor, and the name plate with it, followed the D-pad)
            }
            if (press[0]) { g_popupRow = (g_popupRow + PopupRows() - 1) % PopupRows(); PopupSound(37); }
            if (press[1]) { g_popupRow = (g_popupRow + 1) % PopupRows(); PopupSound(37); }
            if (pressB)
            {
                PopupSound(1);
                OpenCharselPopup(g_popup == kColourStep ? kNoPopup : g_popup - 1);
            }
            else if (pressA)
            {
                PopupSound(0);
                bool monster = variants::Characters()[g_popupPick.character].kind == variants::Kind::Monster;
                if (g_popup == kColourStep && monster) // Germano / Belono: their own animations, no Movement Type / Animation Set (the user, 2026-11-21)
                {
                    g_popupPick.colour = g_popupRow;
                    g_popup = kNoPopup;
                    FinishAdvancedPick();
                }
                else if (g_popup == kColourStep) g_popupPick.colour = g_popupRow, OpenCharselPopup(kDodgeStep);
                else if (g_popup == kDodgeStep) g_popupPick.dodge = g_popupRow, OpenCharselPopup(kSetStep);
                else
                {
                    g_popupPick.set = g_popupRow;
                    g_popup = kNoPopup;
                    FinishAdvancedPick();
                }
            }
            return 0; // the script gets nothing while the pop-up is open
        }

        // The game's window input (FUN_140248b50, per window each frame): reads the pads' pressed / held / repeat words
        // (0x14059b800 + pad * 0x34, + 0 / 4 / 8) for its A / B events and the window's cursor. HookedButtons only filters
        // the script's own read, so on the Advanced list the game's hidden grid cursor still moved under the pop-ups
        // (test 4). There the handler runs with those words zero; pdnet reads the pads itself (gamepads.cpp).
        using WindowInputFn = int64_t (*)(void* window);
        WindowInputFn g_windowInput = nullptr;
        int64_t HookedWindowInput(void* window)
        {
            if (!g_charselAdvanced || !window || CurrentMenu(MenuManager()) != kCharacterMenu) return g_windowInput(window);
            uint32_t* pads = reinterpret_cast<uint32_t*>(At(0x14059b800));
            uint32_t saved[4][3];
            for (int p = 0; p < 4; ++p)
                for (int w = 0; w < 3; ++w) saved[p][w] = pads[p * 13 + w], pads[p * 13 + w] = 0;
            int64_t result = g_windowInput(window);
            for (int p = 0; p < 4; ++p)
                for (int w = 0; w < 3; ++w) pads[p * 13 + w] = saved[p][w];
            return result;
        }

        // g4 #571 stores the character pick (FUN_140213b80: 0x140527bc0 and this PC's lobby slot in 0x140527c20); the
        // Advanced list's pick becomes its carrier's value there, so the lobby and the match see the carrier.
        int64_t HookedStorePick(int32_t* args)
        {
            int slot = PickSlot();
            if (args && slot >= 0 && slot < 4)
            {
                if (int pending = g_pendingPick.exchange(-1); pending >= 0)
                {
                    // the store came (often right after the grid's A): no second A. Test 10: left waiting, it gave up
                    // 5 s later on the next character select, and would have pressed A on a COLORS row there.
                    g_forceConfirm = kConfirmNone;
                    Log(L"variants: Advanced list: the script stores %X for slot %d - pdnet's %X instead", args[0], slot, pending);
                    args[0] = pending;
                    int64_t result = g_storePick(args);
                    LogPickTable("the script's store");
                    RetryMemberVariants("this PC picked");
                    return result;
                }
                if (g_variantPending.exchange(false))
                {
                    int value = ActivateVariant(slot, g_popupPick);
                    Log(L"gamepatch: Advanced list: pick %X -> %X (slot %d)", args[0], value, slot);
                    if (value >= 0) args[0] = value;
                }
                else
                {
                    ClearVariant(slot);
                    PublishVariant(slot, nullptr);
                    CarrierClash(args[0] & 0xffff, slot);
                    Log(L"variants: normal pick %X for slot %d", args[0], slot);
                }
            }
            int64_t stored = g_storePick(args);
            RetryMemberVariants("this PC picked"); // its slot is known now
            return stored;
        }

        // Menu command 29 (0x140207990 -> FUN_14029bb60: a filled rectangle x, y, w, h, colour, drawn at once): the
        // character select's red name plate behind the name, sized by the name's width (command 31). Not on the Advanced list.
        // Test 5: the plate back behind pdnet's name, measured from the game's own (still called every frame, not drawn):
        // its rectangle against the name's width (command 31, FUN_140247550(text, font, 0)) and the name line's place.
        MenuCommandFn g_fillRect = nullptr, g_textWidth = nullptr;
        int64_t g_plate[5] = {}; // the game's last name plate: x, y, w, h, colour
        int g_plateFont = 0, g_plateTextWidth = 0;
        bool g_havePlate = false;
        int64_t HookedFillRect(int64_t* args)
        {
            if (!args || CurrentMenu(MenuManager()) != kCharacterMenu) return g_fillRect(args);
            memcpy(g_plate, args, sizeof(g_plate));
            g_havePlate = g_plateTextWidth > 0;
            static int told = 0;
            if (told++ < 3) Log(L"gamepatch: character select: name plate %lld,%lld %lldx%lld colour %llX (name at %d,%d, width %d)", args[0], args[1], args[2], args[3], args[4], g_gameNameX, g_gameNameY, g_plateTextWidth);
            if (g_charselAdvanced) return 0;
            return g_fillRect(args);
        }
        int64_t HookedTextWidth(int64_t* args)
        {
            int64_t width = g_textWidth(args);
            if (args && CurrentMenu(MenuManager()) == kCharacterMenu) g_plateFont = static_cast<int>(args[1]), g_plateTextWidth = static_cast<int>(width);
            return width;
        }

        // pdnet's name plate (sprites 71, 70 stretched, 72; the handles the game had, g_ownFrames) centred on `centreX`
        // from `y` (sprite coordinates), as wide as `name` (command 31 with the game's font for its name line).
        void DrawNamePlate(const char* name, int centreX, int y)
        {
            uint32_t* list = MnetHandles();
            if (!list || list != g_handles) return;
            int width = static_cast<int>(strlen(name)) * 9;
            if (g_textWidth && g_plateTextWidth > 0)
            {
                int64_t measure[8] = { reinterpret_cast<int64_t>(name), g_plateFont };
                width = static_cast<int>(g_textWidth(measure));
            }
            int inner = width + 8, left = centreX - inner / 2;
            float one = 1.0f, stretch = inner / 8.0f;
            uint32_t white = g_haveGamePlate ? g_gamePlateColour : 0xff401010; // the game's plate colour (test 8: plain white)
            auto draw = [&](uint32_t handle, int x, const float* scaleX)
            {
                reinterpret_cast<SpriteObjectFn>(At(0x1401983e0))(nullptr, handle, x, y, 0, scaleX, &one, &white, 0, 0);
            };
            draw(g_ownFrames[5], left - 20, &one);  // 71 player_name_plate_l
            draw(g_ownFrames[4], left, &stretch);   // 70 player_name_plate_c
            draw(g_ownFrames[6], left + inner, &one); // 72 player_name_plate_r
        }

        // A model's joint by name: FUN_14018aa50 (model, part, base-40 name) binary-searches the part's sorted name table
        // and returns the joint's matrix, or null when the name isn't there. Some skill-effect callers use the result
        // unchecked (0x14018ae75), so a character without a joint a skill names crashes the game (2026-11-21: Piece of
        // Thunder as Ai or Mikan - their models spell "raddh" for "raddha"; variants.cpp now renames it in the built file).
        // This is the safety net for the rest (e.g. Ai has no lteku2 / rnino2 / alpha, imported characters may lack more):
        // a missing name is retried with a stand-in joint, logged once per name (only for the callers that don't check).
        using JointFn = void* (*)(void* model, int part, uint32_t name);
        JointFn g_joint = nullptr;
        struct JointStandIn { uint32_t name; uint32_t standIns[3]; const wchar_t* text; };
        constexpr JointStandIn kJointStandIns[] = {
            { 0xACA1B65B, { 0xACA1B650, 0xADB07630, 0 }, L"raddha -> raddh / rhand" },
            { 0x8802B65B, { 0x8802B650, 0x89117630, 0 }, L"laddha -> laddh / lhand" },
            { 0xACA1B650, { 0xACA1B65B, 0xADB07630, 0 }, L"raddh -> raddha / rhand" },
            { 0x8802B650, { 0x8802B65B, 0x89117630, 0 }, L"laddh -> laddha / lhand" },
            { 0xAF73AAAE, { 0xADB07630, 0, 0 }, L"rsword -> rhand" },
            { 0x8AD4AAAE, { 0x89117630, 0, 0 }, L"lsword -> lhand" },
            { 0x8AEA0E1B, { 0x8AEA0E24, 0, 0 }, L"lteku2 -> ltekub" },
            { 0xAF890E1B, { 0xAF890E24, 0, 0 }, L"rteku2 -> rtekub" },
            { 0x8A03A7EB, { 0x8A03A807, 0, 0 }, L"lnino2 -> lninou" },
            { 0xAEA2A7EB, { 0xAEA2A807, 0, 0 }, L"rnino2 -> rninou" },
        };
        constexpr uint32_t kLastResort[] = { 0x4D1D1738, 0x4D1D1710, 0x70DC9400 }; // body2, body1, hip
        std::mutex g_jointLogLock;
        std::set<uint32_t> g_jointLogged;

        void* HookedJoint(void* model, int part, uint32_t name)
        {
            void* found = g_joint(model, part, name);
            if (found || name == 0 || name == 0xFFFFFFFF) return found;
            // 0x14018976a checks for null itself (a default matrix): left to the game, as unmodded
            if (_ReturnAddress() == At(0x14018976f)) return found;
            const wchar_t* how = L"body2 / body1 / hip (no stand-in for this name)";
            for (const JointStandIn& s : kJointStandIns)
            {
                if (s.name != name) continue;
                how = s.text;
                for (uint32_t other : s.standIns)
                    if (other && (found = g_joint(model, part, other)) != nullptr) break;
                break;
            }
            for (uint32_t other : kLastResort)
                if (!found && (found = g_joint(model, part, other)) != nullptr) break;
            {
                std::lock_guard guard(g_jointLogLock);
                if (g_jointLogged.insert(name).second)
                    Log(L"variants: joint %08X asked for (part %d, from %llX), which this model lacks - stand-in %s%s", name, part,
                        static_cast<unsigned long long>(GhidraAddress(_ReturnAddress())), how, found ? L"" : L": none found either, left null");
            }
            return found;
        }

        void CharacterSelectHooks()
        {
            if (!GetConfig().charselPortraits) return;
            bool sprite = g_drawSprite || WrapCommand<MenuCommandFn>(0x1403eefe0 + 95 * 8, 0x1402077a0, &HookedDrawSprite, g_drawSprite, L"character select portraits");
            bool text = g_drawText || WrapCommand<MenuCommandFn>(0x1403eefe0 + 23 * 8, 0x140207840, &HookedDrawText, g_drawText, L"character select names");
            bool store = WrapCommand<PickFn>(0x1403f0090 + 571 * 8, 0x140213b80, &HookedStorePick, g_storePick, L"Advanced list pick");
            if (!g_textWidth) WrapCommand<MenuCommandFn>(0x1403eefe0 + 31 * 8, 0x140207a20, &HookedTextWidth, g_textWidth, L"character select name width");
            if (!g_fillRect) WrapCommand<MenuCommandFn>(0x1403eefe0 + 29 * 8, 0x140207990, &HookedFillRect, g_fillRect, L"character select name plate");
            // FUN_140248b50 starts push rbx / sub rsp,40h / mov rbx,rcx (9 position-independent bytes)
            static const std::vector<uint8_t> kInputStart = { 0x40, 0x53, 0x48, 0x83, 0xEC, 0x40, 0x48, 0x8B, 0xD9 };
            if (!g_windowInput)
                g_windowInput = reinterpret_cast<WindowInputFn>(InlineHookBytes(0x140248b50, kInputStart, kInputStart, reinterpret_cast<void*>(&HookedWindowInput), L"Advanced list input lock"));
            // FUN_14018aa50 starts sub rsp,8 / mov [rsp],rdi (8 position-independent bytes)
            static const std::vector<uint8_t> kJointStart = { 0x48, 0x83, 0xEC, 0x08, 0x48, 0x89, 0x3C, 0x24 };
            if (!g_joint)
                g_joint = reinterpret_cast<JointFn>(InlineHookBytes(0x14018aa50, kJointStart, kJointStart, reinterpret_cast<void*>(&HookedJoint), L"joint lookup stand-ins"));
            Log(L"gamepatch: character select: monster portraits %s, Advanced list (Y) %s, its picks %s", sprite && text ? L"on" : L"off (hook not set)", sprite ? L"on" : L"off",
                store ? L"on" : L"off");
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
            // (menu_debug: MenuDebug runs at the end of Apply, after every other command wrap.)
        }
    }

    bool OpenLobbyArsenalEditor()
    {
        int64_t manager = MenuManager();
        int current = CurrentMenu(manager);
        if (!g_pushMenu || current != kLobbyMenu) return false;
        Log(L"menu: invite button in the lobby - opening the Arsenal Lab (red hand on until back in the lobby)");
        g_editorFromLobby = true;
        g_lobbyEndedInEditor = false;
        SetLobbyAvailable(false); // like the lobby's own menus: the match can't start while this player is in the Lab
        // The lobby's script uses variable 0 too (0 or 1), so only a value with no list behind it is changed (see
        // MenuVariables): a match or liveend_en.ssb can leave 2 or 3.
        if ((MenuVariables()[0] & 0xff) > 1)
        {
            Log(L"menu: menu variable 0 was %d - 0 for the Arsenal Lab", MenuVariables()[0]);
            MenuVariables()[0] = 0;
        }
        // The Laboratory reads variable 2 as it starts (script command 14) and hands it to g5 #112, which keeps bit 16 at
        // editor state +0xb58: the split-screen "Player 2's copied arsenal" flag. The multiplayer menu zeroes variables
        // 0-2 before the Lab, but here a value left by the lobby's own screens (its arsenal picker, menu 67) could show
        // "Player 2, edits made to the copied arsenal data will only be in effect for the current game session". It is
        // 0 in the Lab and goes back to what the lobby had when the player returns (RestoreLobbyVariable2).
        g_lobbyVariable2 = MenuVariables()[2];
        if (g_lobbyVariable2 != 0) Log(L"menu: menu variable 2 was 0x%X - 0 for the Arsenal Lab (back on return)", g_lobbyVariable2);
        MenuVariables()[2] = 0;
        *At(0x1404bbc34) = 0; // as the script's own "open menu" command does (FUN_140207020)
        g_pushMenu(manager, kLabMenu, 0);
        return true;
    }

    namespace
    {
        // "Rebirth" under the title screen logo. The title screen is Assets\Data\menu\FDLogo.alr: a header page,
        // then 13 textures; the "Phantom Dust" logo is the second (logo_02, 560x96 BGRA, rows of 560 pixels, at 0x4000)
        // and its glow the third (logo_03, 576x112 8-bit). When the game opens the file (CreateFile2, imported by
        // PDUWP.exe) it gets a copy instead, made once per start in LocalState\pdnet: both textures get kSubH rows below
        // for the word, and as many empty rows on top; the header grows with them (heights at 0x60 / 0x7c, 0x38a / 0x39e
        // and 0x4b0 / 0x4ec, later textures' offsets, the data size at 0x14). The title draws both into fixed boxes
        // (logo: y 153..249 in FUN_140279640, glow: y 139..251 in FUN_140278360, one texture row per pixel; each edge
        // a float constant read only there), so those boxes grow by kSubH up and down too (TallerTitleBoxes), else the
        // taller textures come out squashed. The word is the supplied lettering artwork (rebirth_word.h).
        using CreateFile2Fn = HANDLE(WINAPI*)(LPCWSTR, DWORD, DWORD, DWORD, LPCREATEFILE2_EXTENDED_PARAMETERS);
        CreateFile2Fn g_createFile2 = nullptr;

        constexpr int kSubH = 82, kLift = 10; // the word starts kLift rows up, over the logo's own bottom rows
        constexpr float kLogoUp = 48; // the logo, its glow and the word move up together, for room above PRESS ENTER (2026-10-30);
                                      // at most 57: the glow box (139 - kSubH - kLogoUp) has to start inside the target
        constexpr float kPressDown = 60; // "PRESS ENTER TO START" (y 379..395, FUN_140279410) moves down to clear the word
                                         // (no further: the title's offscreen target is 456 rows)

        // The word in kLogoW x (kLift + kSubH) BGRA rows, centred under the logo: the finished lettering from
        // rebirth_word.h (the logo's glow animation lights it through RebirthGlow). The first kLift rows go over the
        // logo's last ones. The logo's art fills columns 0..548 of its 560, so its middle is column 274, not 279.5
        // (centred on the texture, the word sat 5 px right of the logo and of PRESS ENTER, 2026-10-30).
        constexpr int kLogoArtRight = 548;
        std::vector<uint8_t> RebirthRows(int logoW)
        {
            constexpr int h = kLift + kSubH;
            std::vector<uint8_t> rows(logoW * h * 4, 0);
            int ox = (kLogoArtRight + 1 - kRebirthWordW) / 2, oy = (h - kRebirthWordH) / 2;
            if (ox < 0 || ox + kRebirthWordW > logoW) ox = (logoW - kRebirthWordW) / 2;
            for (int y = 0; y < kRebirthWordH; ++y)
                memcpy(&rows[((oy + y) * logoW + ox) * 4], &kRebirthWord[y * kRebirthWordW * 4], kRebirthWordW * 4);
            return rows;
        }

        // The word's glow, like the logo's own: its silhouette widened by 2 px and softened, kGlowW x (word rows + 16)
        // with the word's rows at (8, 8).
        std::vector<uint8_t> RebirthGlow(const std::vector<uint8_t>& rows, int logoW, int glowW)
        {
            int wordH = static_cast<int>(rows.size() / (logoW * 4)), gh = wordH + 16;
            std::vector<uint8_t> a(glowW * gh, 0), b(glowW * gh, 0);
            for (int y = 0; y < wordH; ++y)
                for (int x = 0; x < logoW; ++x) a[(y + 8) * glowW + x + 8] = rows[(y * logoW + x) * 4 + 3];
            for (int y = 0; y < gh; ++y) // widen
                for (int x = 0; x < glowW; ++x)
                {
                    int m = 0;
                    for (int dy = -2; dy <= 2; ++dy)
                        for (int dx = -2; dx <= 2; ++dx)
                        {
                            int yy = y + dy, xx = x + dx;
                            if (yy >= 0 && yy < gh && xx >= 0 && xx < glowW) m = (std::max)(m, static_cast<int>(a[yy * glowW + xx]));
                        }
                    b[y * glowW + x] = static_cast<uint8_t>(m);
                }
            for (int pass = 0; pass < 2; ++pass) // soften: 5x5 box, twice
            {
                for (int y = 0; y < gh; ++y)
                    for (int x = 0; x < glowW; ++x)
                    {
                        int sum = 0;
                        for (int dy = -2; dy <= 2; ++dy)
                            for (int dx = -2; dx <= 2; ++dx)
                            {
                                int yy = y + dy, xx = x + dx;
                                if (yy >= 0 && yy < gh && xx >= 0 && xx < glowW) sum += b[yy * glowW + xx];
                            }
                        a[y * glowW + x] = static_cast<uint8_t>(sum / 25);
                    }
                b.swap(a);
            }
            for (uint8_t& v : b) v = static_cast<uint8_t>(v * 235 / 255);
            return b;
        }

        bool MakeTitleCopy(const wchar_t* original, const std::wstring& copy)
        {
            constexpr DWORD kHeader = 0x1000, kDataSize = 0x27bc80, kSize = kHeader + kDataSize, kLogo = 0x4000, kGlow = 0x38800, kGlowEnd = 0x48400;
            constexpr int kLogoW = 560, kLogoH = 96, kGlowTexW = 576, kGlowTexH = 112;
            constexpr DWORD kExtra = kLogoW * kSubH * 4, kGlowExtra = kGlowTexW * kSubH;
            HANDLE file = g_createFile2(original, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
            if (file == INVALID_HANDLE_VALUE) return false;
            std::vector<uint8_t> data(kSize + 1);
            DWORD got = 0;
            BOOL read = ReadFile(file, data.data(), kSize + 1, &got, nullptr);
            CloseHandle(file);
            auto u32 = [&](size_t at) -> uint32_t& { return *reinterpret_cast<uint32_t*>(&data[at]); };
            auto u16 = [&](size_t at) -> uint16_t& { return *reinterpret_cast<uint16_t*>(&data[at]); };
            if (!read || got != kSize || u32(0x14) != kDataSize || u32(0x54) != kLogo || u32(0x70) != kGlow || u32(0x8c) != kGlowEnd ||
                u32(0x60) != 0x2205f22f || u32(0x7c) != 0x0806f23f || u16(0x38a) != kLogoH || u16(0x4b0) != kLogoH ||
                u16(0x39e) != kGlowTexH || u16(0x4ec) != kGlowTexH || memcmp(&data[0x47c], "logo_02.dds", 11) != 0 ||
                memcmp(&data[0x4b8], "logo_03.dds", 11) != 0)
                return false;

            std::vector<uint8_t> rows = RebirthRows(kLogoW);
            std::vector<uint8_t> wordGlow = RebirthGlow(rows, kLogoW, kGlowTexW);
            std::vector<uint8_t> glow(kGlowExtra, 0);
            glow.insert(glow.end(), &data[kHeader + kGlow], &data[kHeader + kGlowEnd]);
            glow.resize(glow.size() + kGlowExtra, 0);
            // The glow box starts 14 rows above the logo's, so the word (logo row kSubH + 96 - kLift) is glow row
            // kSubH + 110 - kLift, and its glow (the word at row 8 of wordGlow) starts 8 above; past the end is cut.
            const int glowAt = kSubH + 102 - kLift, glowRows = static_cast<int>(wordGlow.size() / kGlowTexW);
            for (int y = 0; y < glowRows && glowAt + y < kGlowTexH + 2 * kSubH; ++y)
                for (int x = 0; x < kGlowTexW; ++x)
                {
                    uint8_t& p = glow[(glowAt + y) * kGlowTexW + x];
                    p = (std::max)(p, wordGlow[y * kGlowTexW + x]);
                }
            // The word's first kLift rows over the logo's last ones (straight-alpha "over").
            for (int y = 0; y < kLift; ++y)
                for (int x = 0; x < kLogoW; ++x)
                {
                    const uint8_t* s = &rows[(y * kLogoW + x) * 4];
                    uint8_t* p = &data[kHeader + kLogo + ((kLogoH - kLift + y) * kLogoW + x) * 4];
                    int a = s[3], under = p[3] * (255 - a) / 255, all = a + under;
                    if (!a) continue;
                    for (int c = 0; c < 3; ++c) p[c] = static_cast<uint8_t>((s[c] * a + p[c] * under) / all);
                    p[3] = static_cast<uint8_t>(all);
                }
            rows.erase(rows.begin(), rows.begin() + kLift * kLogoW * 4);

            std::vector<uint8_t> out(data.begin(), data.begin() + kHeader + kLogo);
            out.resize(out.size() + kExtra, 0);
            out.insert(out.end(), data.begin() + kHeader + kLogo, data.begin() + kHeader + kGlow);
            out.insert(out.end(), rows.begin(), rows.end());
            out.insert(out.end(), glow.begin(), glow.end());
            out.insert(out.end(), data.begin() + kHeader + kGlowEnd, data.begin() + kSize);
            data.swap(out);
            u32(0x14) += 2 * (kExtra + kGlowExtra);
            u32(0x60) = (u32(0x60) & ~0xfff000u) | ((kLogoH + 2 * kSubH - 1) << 12);
            u32(0x7c) = (u32(0x7c) & ~0xfff000u) | ((kGlowTexH + 2 * kSubH - 1) << 12);
            u32(0x70) += 2 * kExtra; // where each later texture starts
            for (int k = 3; k < 13; ++k) u32(0x38 + k * 0x1c) += 2 * (kExtra + kGlowExtra);
            u16(0x38a) = u16(0x4b0) = kLogoH + 2 * kSubH;
            u16(0x39e) = u16(0x4ec) = kGlowTexH + 2 * kSubH;
            CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
            file = g_createFile2(copy.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
            if (file == INVALID_HANDLE_VALUE) return false;
            DWORD written = 0;
            BOOL ok = WriteFile(file, data.data(), static_cast<DWORD>(data.size()), &written, nullptr) && written == data.size();
            CloseHandle(file);
            return ok;
        }

        // The logo and glow boxes' top and bottom edges (see above), moved out by kSubH, and "PRESS ENTER TO START"
        // moved down (its edges also read only there); only if all are as expected.
        bool TallerTitleBoxes()
        {
            struct Edge { uint64_t at; float was, now; };
            constexpr Edge kEdges[] = { { 0x1402f80a8, 153.03125f, 153.03125f - kSubH - kLogoUp }, { 0x1402f810c, 249.03125f, 249.03125f + kSubH - kLogoUp },
                                        { 0x1402f8090, 139.03125f, 139.03125f - kSubH - kLogoUp }, { 0x1402f8114, 251.03125f, 251.03125f + kSubH - kLogoUp },
                                        { 0x1402f8168, 379.03125f, 379.03125f + kPressDown }, { 0x1402f8170, 395.03125f, 395.03125f + kPressDown } };
            for (const Edge& e : kEdges)
                if (*reinterpret_cast<const float*>(At(e.at)) != e.was) return false;
            for (const Edge& e : kEdges)
            {
                std::vector<uint8_t> bytes(4);
                memcpy(bytes.data(), &e.now, 4);
                if (!Write(At(e.at), bytes)) return false;
            }
            return true;
        }


        // Character select portraits (2026-11-20, the user's art; tools\charsel\make_portraits.py). The character select
        // (livecharasel, menu 46) draws a 134 x 96 portrait with menu command 95 from MNET (Assets\Data\menu\MNet.alr,
        // texture slot 3): sprites 38-48 (player_00..player_10). The file has no large monster portraits (only the small window_chara_11..26), so the X toggle
        // (Toggle Monsters) only changes the names. When the game opens the file it gets a copy (LocalState\pdnet\MNet.alr,
        // made once per start) with kit\pdnet\charsel-portraits.bin put in (a file, not a folder: the updater copies only
        // files):
        // - MNet0800 (texture 9, 256 x 512 DXT5) widened to 1024 x 512: the game's own quarter on the left, the monster
        //   portraits right of it. The players keep the game's own portraits (the user, 2026-11-20). The file format can
        //   also replace a whole texture or a game sprite's art (tools\charsel\make_portraits.py).
        // - The sprite list (the 0x10 record at 0x198: 12 texture infos of 0x14 at 0x338, 84 sprites of 0x3c at 0x428:
        //   texture, name[32], u0 v0 u1 v1, w, h): the 11 monsters added as sprites 84-94.
        //   The record and the sprite count (0x1a4) grow; the record's end marker moves into the padding before 0x2000.
        // - Texture table (0x34 + 0x1c each: +4 offset in the data, +0xe log2 width << 4 | mips) and the data size (0x14).
        // charsel_portraits=off leaves the game's file alone.

        std::wstring PdnetDir()
        {
            HMODULE self = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(&PdnetDir), &self);
            wchar_t path[MAX_PATH];
            GetModuleFileNameW(self, path, MAX_PATH);
            std::wstring dir = path;
            return dir.substr(0, dir.find_last_of(L'\\') + 1);
        }

        bool ReadWholeFile(const std::wstring& path, std::vector<uint8_t>& data, size_t limit)
        {
            HANDLE file = g_createFile2(path.c_str(), GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING, nullptr);
            if (file == INVALID_HANDLE_VALUE) return false;
            LARGE_INTEGER size{};
            bool ok = GetFileSizeEx(file, &size) && size.QuadPart > 0 && static_cast<uint64_t>(size.QuadPart) <= limit;
            DWORD got = 0;
            if (ok)
            {
                data.resize(static_cast<size_t>(size.QuadPart));
                ok = ReadFile(file, data.data(), static_cast<DWORD>(data.size()), &got, nullptr) && got == data.size();
            }
            CloseHandle(file);
            return ok;
        }

        // Returns the number of sprites added, or -1 (file left as it is; `why` says what didn't match).
        int MakeMnetCopy(const wchar_t* original, const std::wstring& copy, const char*& why)
        {
            constexpr size_t kSize = 863232;
            constexpr uint32_t kData = 0x2000; // the data's start (header +0xc)
            constexpr uint32_t kTex = 0x34, kInfo = 0x338, kSprites = 0x428, kSprite = 0x3c;
            constexpr uint32_t kEnd = kSprites + kMnetGameSprites * kSprite;
            std::vector<uint8_t> d, p;
            why = "MNet.alr not read";
            if (!ReadWholeFile(original, d, kSize)) return -1;
            auto u32 = [&](size_t at) -> uint32_t& { return *reinterpret_cast<uint32_t*>(&d[at]); };
            auto u16 = [&](size_t at) -> uint16_t& { return *reinterpret_cast<uint16_t*>(&d[at]); };
            auto f32 = [&](size_t at) -> float& { return *reinterpret_cast<float*>(&d[at]); };
            why = "MNet.alr not the expected file";
            if (d.size() != kSize || u32(0) != 0x11 || u32(0xc) != kData || u32(0x14) != kSize - kData || u32(0x28) != 0x15 || u32(0x30) != 12 ||
                u32(0x198) != 0x10 || u32(0x19c) != 0x1640 || u32(0x1a4) != kMnetGameSprites || memcmp(&d[0x2d8], "MNet0800.dds", 12) != 0 ||
                u32(kEnd) != 0 || u32(kEnd + 4) != 8)
                return -1;
            why = "kit\\pdnet\\charsel-portraits.bin missing or not readable";
            if (!ReadWholeFile(PdnetDir() + L"charsel-portraits.bin", p, 8 << 20) || p.size() < 16 || memcmp(p.data(), "PDCP", 4) != 0 ||
                *reinterpret_cast<uint32_t*>(&p[4]) != 1)
                return -1;
            // The sprite list must end before 0x2000 (115 sprites): a test build that moved the data to 0x3000 for more
            // (2026-11-21) crashed the game on the online menu. The lobby banners therefore have no sprites of their own
            // (LobbyBanner).
            why = "charsel-portraits.bin doesn't fit MNet.alr";
            uint32_t images = *reinterpret_cast<uint32_t*>(&p[8]), sprites = *reinterpret_cast<uint32_t*>(&p[12]);
            size_t at = 16;
            for (uint32_t i = 0; i < images; ++i)
            {
                if (at + 16 > p.size()) return -1;
                uint32_t tex = *reinterpret_cast<uint32_t*>(&p[at]), mode = *reinterpret_cast<uint32_t*>(&p[at + 8]), size = *reinterpret_cast<uint32_t*>(&p[at + 12]);
                uint32_t w = *reinterpret_cast<uint16_t*>(&p[at + 4]), h = *reinterpret_cast<uint16_t*>(&p[at + 6]);
                const uint8_t* art = &p[at + 16];
                at += 16 + size;
                if (at > p.size() || tex >= 12 || size != w * h) return -1;
                size_t entry = kTex + tex * 0x1c, info = kInfo + tex * 0x14;
                uint32_t oldW = u16(info), oldH = u16(info + 2), offset = u32(entry + 4), oldSize = oldW * oldH; // DXT5: a byte per pixel
                if (d[entry + 0xd] != 0x0f || kData + offset + oldSize > d.size()) return -1;
                std::vector<uint8_t> image;
                if (mode == 1) // added to the right: rows of 4 x 4 blocks (16 bytes a block) side by side
                {
                    uint32_t newW = oldW + w;
                    if (h != oldH || (newW & (newW - 1)) != 0 || h % 4) return -1;
                    for (uint32_t row = 0; row < h / 4; ++row)
                    {
                        const uint8_t* own = &d[kData + offset + row * oldW * 4];
                        image.insert(image.end(), own, own + oldW * 4);
                        image.insert(image.end(), art + row * w * 4, art + (row + 1) * w * 4);
                    }
                    unsigned long log2 = 0;
                    _BitScanReverse(&log2, newW);
                    d[entry + 0xe] = static_cast<uint8_t>(log2 << 4 | (d[entry + 0xe] & 0xf));
                    u16(info) = static_cast<uint16_t>(newW);
                    memcpy(&d[info + 4], &d[kInfo + 4], 4); // as MNet0400's info, also 1024 x 512
                    for (int k = 0; k < kMnetGameSprites; ++k) // the game's sprites on it keep their pixels
                        if (u32(kSprites + k * kSprite) == tex)
                        {
                            f32(kSprites + k * kSprite + 36) *= static_cast<float>(oldW) / newW;
                            f32(kSprites + k * kSprite + 44) *= static_cast<float>(oldW) / newW;
                        }
                }
                else
                {
                    if (w != oldW || h != oldH) return -1;
                    image.assign(art, art + size);
                }
                uint32_t grow = static_cast<uint32_t>(image.size()) - oldSize;
                d.erase(d.begin() + kData + offset, d.begin() + kData + offset + oldSize);
                d.insert(d.begin() + kData + offset, image.begin(), image.end());
                u32(0x14) += grow;
                for (int k = 0; k < 12; ++k)
                    if (u32(kTex + k * 0x1c + 4) > offset) u32(kTex + k * 0x1c + 4) += grow;
            }
            uint8_t end[8];
            memcpy(end, &d[kEnd], 8);
            int added = 0;
            for (uint32_t i = 0; i < sprites; ++i, at += 44)
            {
                if (at + 44 > p.size()) return -1;
                char name[33] = {};
                memcpy(name, &p[at], 32);
                uint32_t tex = *reinterpret_cast<uint32_t*>(&p[at + 32]);
                const uint16_t* box = reinterpret_cast<const uint16_t*>(&p[at + 36]);
                if (tex >= 12) return -1;
                float texW = u16(kInfo + tex * 0x14), texH = u16(kInfo + tex * 0x14 + 2);
                int k = 0;
                while (k < kMnetGameSprites && strncmp(reinterpret_cast<const char*>(&d[kSprites + k * kSprite + 4]), name, 32) != 0) ++k;
                if (k == kMnetGameSprites)
                {
                    k = kMnetGameSprites + added++;
                    if (kSprites + (k + 1) * kSprite + 8 > kData) return -1; // room for 115 sprites in all (112 used, 2026-11-21)
                    for (uint32_t b = kSprites + k * kSprite + 8; b < kSprites + (k + 1) * kSprite + 8; ++b)
                        if (d[b]) return -1; // only padding may be there
                }
                size_t s = kSprites + k * kSprite;
                u32(s) = tex;
                memcpy(&d[s + 4], name, 32);
                f32(s + 36) = box[0] / texW, f32(s + 40) = box[1] / texH, f32(s + 44) = (box[0] + box[2]) / texW, f32(s + 48) = (box[1] + box[3]) / texH;
                u32(s + 52) = box[2], u32(s + 56) = box[3];
            }
            memcpy(&d[kEnd + added * kSprite], end, 8);
            u32(0x19c) += added * kSprite;
            u32(0x1a4) += added;
            g_mnetSprites.assign(&d[kSprites], &d[kSprites + (kMnetGameSprites + added) * kSprite]);
            uint32_t counts[12] = {}; // each texture's info counts the sprites on it
            for (int k = 0; k < kMnetGameSprites + added; ++k)
                if (u32(kSprites + k * kSprite) < 12) ++counts[u32(kSprites + k * kSprite)];
            for (int t = 0; t < 12; ++t) u32(kInfo + t * 0x14 + 8) = counts[t];

            why = "the copy couldn't be written";
            CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
            HANDLE file = g_createFile2(copy.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
            if (file == INVALID_HANDLE_VALUE) return -1;
            DWORD written = 0;
            BOOL ok = WriteFile(file, d.data(), static_cast<DWORD>(d.size()), &written, nullptr) && written == d.size();
            CloseHandle(file);
            return ok ? added : -1;
        }


        // Character variants (variants.h; the Advanced list, docs/CHARACTER_SELECT.md). A picked variant plays as a
        // "carrier": a charaset record the normal lists don't offer, one per lobby slot, so every PC can tell whose
        // files are whose (kCarriers). While a slot's variant is active:
        // - the game's files for the carrier (Player\<folder>\<folder><colour>.alr, exmot0..5.ala) are built by
        //   variants::BuildModel / BuildExmot from the game's own files and handed over instead (LocalState\pdnet\variants);
        // - the carrier's charaset entry in memory gets the variant's movement skill, voice, exmot lengths and hitbox.
        //   The game unpacks charaset into 0xa0-byte entries at 0x1404c01e0 (FUN_14017d300; count at 0x1404c0174):
        //   +0 model folder, +0x18..+0x24 capsule height, width, hit radius, hit centre (file value / [0x1402f7f7c]),
        //   +0x28..+0x68, +0x74, +0x78 movement stats (turning, speeds; taken from the animation set's character),
        //   +0x6C movement skill, +0x80 voice bank name, +0x88 six exmot lengths (ints).
        // variant_test=<character>,<colour>,<dodge>,<set> (pdnet.ini, numbers from variants.cpp's lists) plays your own
        // Test Arsenal character as that variant, before the menu exists.
        struct Carrier { int record; const char* folder; };
        constexpr Carrier kCarriers[4] = { { 19, "enm04a" }, { 21, "enm06a" }, { 22, "enm07a" }, { 26, "enm15a" } }; // Mechano, Euroto, Hedono, Belono
        struct ActiveVariant { bool on = false; variants::Pick pick; uint8_t saved[0xa0]; };
        ActiveVariant g_variants[4];
        std::mutex g_variantLock;
        std::wstring g_dataPrefix = L"Assets\\Data\\"; // how the game names its data folder (taken from its own file opens)

        uint8_t* CharasetEntry(int record) { return At(0x1404c01e0) + static_cast<size_t>(record) * 0xa0; }

        std::vector<uint8_t> ReadGameFile(const std::string& relative)
        {
            std::vector<uint8_t> data;
            ReadWholeFile(g_dataPrefix + std::wstring(relative.begin(), relative.end()), data, 64 << 20);
            return data;
        }

        bool CarrierFolderIs(int slot)
        {
            int count = *reinterpret_cast<int32_t*>(At(0x1404c0174));
            const char* folder = *reinterpret_cast<const char**>(CharasetEntry(kCarriers[slot].record));
            char text[16];
            return kCarriers[slot].record < count && folder && CopyString(folder, text, sizeof(text)) && strcmp(text, kCarriers[slot].folder) == 0;
        }

        void ClearVariant(int slot)
        {
            std::lock_guard guard(g_variantLock);
            if (!g_variants[slot].on) return;
            memcpy(CharasetEntry(kCarriers[slot].record), g_variants[slot].saved, 0xa0);
            g_variants[slot].on = false;
            Log(L"variants: slot %d back to the game's %S", slot, kCarriers[slot].folder);
        }

        // The game value for the match setup (colour << 16 | record), or -1.
        int ActivateVariant(int slot, const variants::Pick& pick)
        {
            const auto& list = variants::Characters();
            if (slot < 0 || slot >= 4 || pick.character < 0 || pick.character >= static_cast<int>(list.size())) return -1;
            if (!CarrierFolderIs(slot))
            {
                Log(L"variants: carrier record %d isn't %S - variants off", kCarriers[slot].record, kCarriers[slot].folder);
                return -1;
            }
            ClearVariant(slot);
            std::lock_guard guard(g_variantLock);
            ActiveVariant& v = g_variants[slot];
            uint8_t* e = CharasetEntry(kCarriers[slot].record);
            memcpy(v.saved, e, 0xa0);
            const variants::Character& c = list[pick.character];
            // hitbox: the character's own (its record, or the saved original if that record is a carrier now), height at least 18
            // Ai and Mikan (charaset height 16) get Freia's hitbox exactly (the user, 2026-11-21: played competitively, no
            // smaller target than Freia's).
            int hitboxRecord = c.record;
            if (strcmp(c.name, "Ai") == 0 || strcmp(c.name, "Mikan") == 0) hitboxRecord = 2; // Freia
            const uint8_t* own = hitboxRecord >= 0 ? CharasetEntry(hitboxRecord) : nullptr;
            for (int s = 0; s < 4; ++s)
                if (g_variants[s].on && hitboxRecord == kCarriers[s].record) own = g_variants[s].saved;
            if (own) memcpy(e + 0x18, own + 0x18, 16);
            float minHeight = 180.0f / *reinterpret_cast<float*>(At(0x1402f7f7c));
            // Ubiquitous (both models, 12) and Kei (14) keep their own small hitbox: meme picks (the user, 2026-11-21)
            bool meme = strncmp(c.name, "Ubiquitous", 10) == 0 || strcmp(c.name, "Kei") == 0;
            if (!meme && *reinterpret_cast<float*>(e + 0x18) < minHeight) *reinterpret_cast<float*>(e + 0x18) = minHeight;
            const float* box = reinterpret_cast<const float*>(e + 0x18);
            Log(L"variants: slot %d hitbox %.1f / %.1f / %.1f / %.1f (from record %d%s)", slot, box[0], box[1], box[2], box[3], hitboxRecord,
                hitboxRecord != c.record ? L", Freia's" : L"");
            // Movement stats (turning, speeds...: +0x28..+0x68, +0x74, +0x78, from the file's +0x14..+0x34, +0x38, +0x3A) from
            // the animation set's character: test 2 (2026-11-20) kept the carrier's (Mechano: turning as slow as
            // Catoptro's) with Gyne's animations; Gyne turns like Meister.
            const uint8_t* stats = CharasetEntry(variants::AnimationSets()[pick.set].record);
            for (int s = 0; s < 4; ++s)
                if (g_variants[s].on && variants::AnimationSets()[pick.set].record == kCarriers[s].record) stats = g_variants[s].saved;
            memcpy(e + 0x28, stats + 0x28, 0x6c - 0x28);
            memcpy(e + 0x74, stats + 0x74, 8);
            *reinterpret_cast<int32_t*>(e + 0x6c) = variants::Dodges()[pick.dodge].move;
            *reinterpret_cast<const char**>(e + 0x80) = c.voice; // static strings in pdnet
            int16_t lengths[6];
            variants::SetLengths(pick, lengths);
            for (int k = 0; k < 6; ++k) reinterpret_cast<int32_t*>(e + 0x88)[k] = lengths[k];
            v.pick = pick;
            v.on = true;
            Log(L"variants: slot %d plays %S (%S, %S dodge, %S set) as %S, movement %d, voice %S", slot, c.name,
                c.colours[pick.colour < variants::ColourCount(c) ? pick.colour : 0], variants::Dodges()[pick.dodge].name,
                variants::AnimationSets()[pick.set].name, kCarriers[slot].folder, variants::Dodges()[pick.dodge].move, c.voice);
            return (pick.colour & 0xffff) << 16 | kCarriers[slot].record;
        }

        // A variant's file: the model (exmot -1, Player\<carrier>\<carrier><colour>.alr) or exmot0..5.ala, built once per
        // pick and session into LocalState\pdnet\variants (PrebuildVariant builds them in the background as soon as a pick
        // is known, so the match load only opens them). Empty if the build failed (the game's own file is used then).
        std::mutex g_buildLock;
        std::set<std::wstring> g_builtFiles;
        std::wstring BuildVariantFile(const variants::Pick& pick, int exmot, const wchar_t* forName)
        {
            std::string key = std::to_string(pick.character) + "-" + std::to_string(pick.dodge) + "-" + std::to_string(pick.set);
            std::string out = exmot >= 0 ? key + "-exmot" + std::to_string(exmot) + ".ala" : key + "-" + std::to_string(pick.colour) + ".alr";
            std::wstring dir = LocalStatePath() + L"\\pdnet\\variants";
            std::wstring target = dir + L"\\" + std::wstring(out.begin(), out.end());
            std::lock_guard guard(g_buildLock); // one build at a time (variants:: keeps its stats in statics); waits for a running prebuild
            if (g_builtFiles.count(target))
            {
                Log(L"variants: %s -> %s (built earlier this session)", forName, target.c_str());
                return target;
            }
            std::string error;
            ULONGLONG started = GetTickCount64();
            std::vector<uint8_t> bytes = exmot >= 0 ? variants::BuildExmot(pick, exmot, ReadGameFile, error) : variants::BuildModel(pick, ReadGameFile, error);
            if (bytes.empty())
            {
                Log(L"variants: %s - build FAILED, the game's own file stays (%S; data folder '%s')", forName, error.c_str(), g_dataPrefix.c_str());
                return L"";
            }
            CreateDirectoryW((LocalStatePath() + L"\\pdnet").c_str(), nullptr);
            CreateDirectoryW(dir.c_str(), nullptr);
            HANDLE f = g_createFile2(target.c_str(), GENERIC_WRITE, 0, CREATE_ALWAYS, nullptr);
            if (f == INVALID_HANDLE_VALUE)
            {
                Log(L"variants: %s - could not write %s (error %lu)", forName, target.c_str(), GetLastError());
                return L"";
            }
            DWORD written = 0;
            BOOL ok = WriteFile(f, bytes.data(), static_cast<DWORD>(bytes.size()), &written, nullptr) && written == bytes.size();
            CloseHandle(f);
            variants::Stats st = variants::LastStats();
            Log(L"variants: %s -> %s (%u bytes, %llu ms; %d runs, %d tracks, %d dropped, %d joints, %d renamed, %d stray draws hidden)%s", forName, target.c_str(), static_cast<unsigned>(bytes.size()),
                GetTickCount64() - started, st.runs, st.tracks, st.dropped, st.joints, st.renamed, st.hidden, ok ? L"" : L" - WRITE FAILED");
            if (!ok) return L"";
            g_builtFiles.insert(target);
            return target;
        }

        // All of a pick's files, on a worker thread (logged with their times).
        void PrebuildVariant(int slot, const variants::Pick& pick, const char* why)
        {
            std::thread([slot, pick, why]
            {
                ULONGLONG started = GetTickCount64();
                const char* name = variants::Characters()[pick.character].name;
                Log(L"variants: prebuild for slot %d (%S): %S colour %d, %S, %S set - started", slot, why, name, pick.colour,
                    variants::Dodges()[pick.dodge].name, variants::AnimationSets()[pick.set].name);
                int ok = BuildVariantFile(pick, -1, L"(prebuild model)").empty() ? 0 : 1;
                for (int k = 0; k < 6; ++k)
                {
                    wchar_t label[32];
                    swprintf_s(label, L"(prebuild exmot%d)", k);
                    ok += BuildVariantFile(pick, k, label).empty() ? 0 : 1;
                }
                Log(L"variants: prebuild for slot %d (%S) done: %d of 7 files, %llu ms", slot, name, ok, GetTickCount64() - started);
            }).detach();
        }

        // What the game has picked per lobby slot (0x140527c20, colour << 16 | record) and which variants are on.
        void LogPickTable(const char* why)
        {
            const int32_t* picks = reinterpret_cast<const int32_t*>(At(0x140527c20));
            std::wstring line;
            for (int s = 0; s < 4; ++s)
            {
                wchar_t part[160];
                std::lock_guard guard(g_variantLock);
                const ActiveVariant& v = g_variants[s];
                swprintf_s(part, L" | slot %d: %X%s%S", s, picks[s], v.on ? L" variant " : L"", v.on ? variants::Characters()[v.pick.character].name : "");
                line += part;
            }
            Log(L"variants: pick table (%S), this PC's slot %d%s", why, *reinterpret_cast<int32_t*>(At(0x140527bc4)), line.c_str());
        }

        // The lobby's banner (window_chara_NN: sprite NN = the pick's charaset record) of a carrier whose slot has a variant:
        // the character's own banner (players), or an NPC's banner from tools\charsel\make_portraits.py. Those banners are
        // art in MNet0800 (texture 9) without sprites of their own (MNet.alr has no room): each lobby slot shows its banner
        // through one of the game's unused sprites dummy001-004 (27-30; charsel-portraits.bin gives them texture 9 and a
        // 74 x 24 box), whose source rectangle pdnet moves to banner k. The rectangle table: [0x140527c78] + 0x3a8 + group *
        // 8 -> per object -> idx * 8: s16 x, y, w, h (FUN_1401983e0; handle = group << 16 | object << 8 | idx). -1: the game's.
        constexpr int kLobbyBannerSprite = 27;
        int16_t* SpriteRect(uint32_t handle)
        {
            __try
            {
                uint8_t* base = *reinterpret_cast<uint8_t**>(At(0x140527c78));
                uint8_t* groups = *reinterpret_cast<uint8_t**>(base + 0x3a8 + ((handle >> 16) & 0xff) * 8);
                uint8_t* rects = *reinterpret_cast<uint8_t**>(groups + ((handle >> 8) & 0xff) * 8);
                return reinterpret_cast<int16_t*>(rects + (handle & 0xff) * 8);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
        }

        int BannerNumber(const variants::Character& c)
        {
            static const char* kBanners[13] = { "Spokesman", "Mikan", "Ai", "Kei", "Kajikawa", "03", "Maniac", "Arthur", "Mac", "Baroness",
                                                "Tetsuya", "Reindeer", "Ubiquitous" };
            if (c.banner >= 0) return c.banner; // an added character's (sprites.txt "banner k")
            std::string name = c.name;
            name = name.substr(0, name.find(' ')); // "Ubiquitous Keyboard"
            for (int k = 0; k < 13; ++k)
                if (name == kBanners[k]) return k;
            return -1;
        }

        int LobbyBanner(int sprite)
        {
            for (int s = 0; s < 4; ++s)
            {
                if (kCarriers[s].record != sprite || !g_variants[s].on) continue;
                const variants::Character& c = variants::Characters()[g_variants[s].pick.character];
                if (c.kind == variants::Kind::Player && c.record >= 0 && c.record <= 10 && c.banner < 0) return c.record;
                int k = BannerNumber(c);
                uint32_t* list = MnetHandles();
                if (k < 0 || !list || g_monsterPortraits <= 0) return -1;
                int16_t* rect = SpriteRect(list[kLobbyBannerSprite + s]);
                if (!rect) return -1;
                int16_t x = static_cast<int16_t>(256 + (k % 23) * 76), y = static_cast<int16_t>(294 + (k / 23) * 26);
                static int told = 0;
                if (told < 8 && (rect[0] != x || rect[1] != y))
                {
                    ++told;
                    Log(L"variants: lobby banner of slot %d: %S (banner %d) - sprite %d's rectangle %d,%d %dx%d -> %d,%d", s, c.name, k,
                        kLobbyBannerSprite + s, rect[0], rect[1], rect[2], rect[3], x, y);
                }
                __try { rect[0] = x, rect[1] = y; }
                __except (EXCEPTION_EXECUTE_HANDLER) { return -1; }
                return kLobbyBannerSprite + s;
            }
            return -1;
        }

        // The carriers' window_chara handles in MNET's list point at their slot's banner sprite while the slot has a variant
        // (and the game's own otherwise), for draws that don't come through command 95.
        void LobbyBannerHandles()
        {
            static uint32_t* list = nullptr;
            static uint32_t own[4];
            uint32_t* now = MnetHandles();
            if (!now) return;
            if (now != list)
            {
                list = now;
                for (int s = 0; s < 4; ++s) own[s] = list[kCarriers[s].record];
                Log(L"variants: lobby banners: carrier handles %X %X %X %X", own[0], own[1], own[2], own[3]);
            }
            for (int s = 0; s < 4; ++s)
            {
                int shown = LobbyBanner(kCarriers[s].record); // sets the slot sprite's rectangle; -1 = the game's
                uint32_t want = shown < 0 ? own[s] : list[shown];
                if (list[kCarriers[s].record] != want)
                {
                    list[kCarriers[s].record] = want;
                    Log(L"variants: lobby banners: slot %d's carrier sprite %d now shows sprite %d", s, kCarriers[s].record, shown < 0 ? kCarriers[s].record : shown);
                }
            }
        }

        // A normal / monster pick of a record that is some slot's carrier while that slot's variant is on: that player
        // would show the variant (Belono is slot 3's carrier). Only logged for now.
        void CarrierClash(int record, int slot)
        {
            for (int s = 0; s < 4; ++s)
                if (s != slot && record == kCarriers[s].record && g_variants[s].on)
                    Log(L"variants: WARNING slot %d picked record %d, the carrier of slot %d's variant (%S): it will look like that variant", slot, record, s,
                        variants::Characters()[g_variants[s].pick.character].name);
        }

        // ---- Online: each PC shares its pick as its lobby member attribute PD_VARIANT, "1|slot|character|colour|dodge|set"
        // (names, not list numbers, so a mismatch shows), empty for a normal pick. xsapi/mpsd.cpp adds it to every member
        // write (VariantAttribute) and hands every lobby update to LobbyVariants, which turns the other members' picks
        // on for their slots (and builds their files in the background).
        std::mutex g_shareLock;
        std::string g_myVariant, g_variantLobby;
        std::map<std::string, std::string> g_memberVariant; // EOS user id -> PD_VARIANT last applied
        std::map<std::string, int> g_memberSlot;            // EOS user id -> the slot their variant was applied to
        // Toaf's test (2026-11-21): when this PC joined, its own slot still read 0 (the game sets it later) and Nui's
        // PD_VARIANT for slot 0 was refused as "this PC's own slot" - and never looked at again. Refused ones wait here
        // (EOS user id -> PD_NAME) and are tried again after this PC's own pick and when the match loads player files.
        std::map<std::string, std::string> g_memberRetry;
        std::atomic<bool> g_anyRetry{ false };

        std::string VariantText(int slot, const variants::Pick& k)
        {
            return "1|" + std::to_string(slot) + "|" + variants::Characters()[k.character].name + "|" + std::to_string(k.colour) + "|" +
                   variants::Dodges()[k.dodge].name + "|" + variants::AnimationSets()[k.set].name;
        }

        bool ParseVariant(const std::string& text, int& slot, variants::Pick& k, std::string& why)
        {
            std::vector<std::string> f;
            for (size_t at = 0;;)
            {
                size_t bar = text.find('|', at);
                f.push_back(text.substr(at, bar == std::string::npos ? std::string::npos : bar - at));
                if (bar == std::string::npos) break;
                at = bar + 1;
            }
            if (f.size() != 6 || f[0] != "1") { why = "not version 1 / 6 fields"; return false; }
            slot = atoi(f[1].c_str());
            k = {};
            k.colour = atoi(f[3].c_str());
            for (size_t i = 0; i < variants::Characters().size(); ++i) if (f[2] == variants::Characters()[i].name) k.character = static_cast<int>(i);
            for (size_t i = 0; i < variants::Dodges().size(); ++i) if (f[4] == variants::Dodges()[i].name) k.dodge = static_cast<int>(i);
            int set = -1;
            for (size_t i = 0; i < variants::AnimationSets().size(); ++i) if (f[5] == variants::AnimationSets()[i].name) set = static_cast<int>(i);
            if (slot < 0 || slot > 3) { why = "slot out of range"; return false; }
            if (k.character < 0) { why = "unknown character '" + f[2] + "' (older / newer pdnet?)"; return false; }
            if (set < 0) { why = "unknown animation set '" + f[5] + "'"; return false; }
            k.set = set;
            if (k.colour < 0 || k.colour > 3) k.colour = 0;
            return true;
        }

        void PublishVariant(int slot, const variants::Pick* pick)
        {
            if (g_testPicking != kPickingNone) return; // Test Arsenal: nobody to tell
            std::string text = pick ? VariantText(slot, *pick) : "", lobby;
            {
                std::lock_guard guard(g_shareLock);
                if (text == g_myVariant) return;
                g_myVariant = text;
                lobby = g_variantLobby;
            }
            if (lobby.empty())
            {
                Log(L"variants: share: no game lobby known yet - '%S' goes out with this PC's next lobby write", text.c_str());
                return;
            }
            std::thread([lobby, text]
            {
                lobbies::Changes member;
                if (text.empty()) member.remove.push_back("PD_VARIANT");
                else member.set["PD_VARIANT"] = text;
                bool ok = lobbies::Update(lobby, lobbies::Changes{}, member);
                Log(L"variants: share: PD_VARIANT '%S' to lobby %S - %s", text.c_str(), lobby.c_str(), ok ? L"ok" : L"FAILED");
            }).detach();
        }

        // Another member's PD_VARIANT changed (or they left: `text` empty).
        void ApplyMemberVariant(const std::string& user, const std::string& name, const std::string& text)
        {
            int mySlot = *reinterpret_cast<int32_t*>(At(0x140527bc4));
            int oldSlot = g_memberSlot.count(user) ? g_memberSlot[user] : -1;
            if (text.empty())
            {
                g_memberRetry.erase(user);
                if (oldSlot >= 0 && oldSlot != mySlot) ClearVariant(oldSlot);
                g_memberSlot.erase(user);
                Log(L"variants: online: %S (%S) plays a normal pick now%s", name.c_str(), user.c_str(), oldSlot >= 0 ? L" - their slot's variant is off" : L"");
                return;
            }
            int slot = -1;
            variants::Pick pick;
            std::string why;
            if (!ParseVariant(text, slot, pick, why))
            {
                Log(L"variants: online: %S's PD_VARIANT '%S' not used: %S", name.c_str(), text.c_str(), why.c_str());
                return;
            }
            if (slot == mySlot)
            {
                Log(L"variants: online: %S says slot %d, which this PC reads as its own slot (yet) - tried again later", name.c_str(), slot);
                g_memberRetry[user] = name;
                g_anyRetry = true;
                return;
            }
            g_memberRetry.erase(user);
            if (oldSlot >= 0 && oldSlot != slot && oldSlot != mySlot) ClearVariant(oldSlot); // they moved slots
            int value = ActivateVariant(slot, pick);
            g_memberSlot[user] = slot;
            Log(L"variants: online: %S (%S) picked %S for slot %d -> carrier value %X%s", name.c_str(), user.c_str(), text.c_str(), slot, value,
                value < 0 ? L" - NOT ACTIVE" : L"");
            if (value >= 0) PrebuildVariant(slot, pick, "another player's pick");
            LogPickTable("after another player's pick");
        }

        // Members whose PD_VARIANT waited (g_memberRetry), tried again now that this PC's slot may be known.
        void RetryMemberVariants(const char* why)
        {
            if (!g_anyRetry) return;
            std::lock_guard guard(g_shareLock);
            std::map<std::string, std::string> waiting;
            waiting.swap(g_memberRetry);
            g_anyRetry = false;
            Log(L"variants: online: trying %zu waiting pick(s) again (%S), this PC's slot %d", waiting.size(), why, *reinterpret_cast<int32_t*>(At(0x140527bc4)));
            for (auto& [user, name] : waiting)
                if (auto it = g_memberVariant.find(user); it != g_memberVariant.end() && !it->second.empty()) ApplyMemberVariant(user, name, it->second);
        }

        // ---- Theatre films (the user, 2026-11-21): an Advanced pick plays as its slot's carrier (kCarriers), so a film
        // recorded only the carrier value (colour << 16 | 19 / 21 / 22 / 26) and its battle loaded the carrier monster with
        // none of the variant's files, which crashed the Theatre. Films now record each slot's PD_VARIANT text (snapshot
        // version 4, film::Info::variant) and the film's battle turns those variants on for their slots before it loads
        // (ApplyFilmVariants), building their files in the background as for another player's pick. They go off again when
        // the menus come back after the film (ClearFilmVariants).
        uint32_t g_filmVariantSlots = 0; // bit per slot turned on for a film

        bool IsCarrierValue(int value, int& slot)
        {
            for (int s = 0; s < 4; ++s)
                if ((value & 0xffff) == kCarriers[s].record) { slot = s; return true; }
            return false;
        }

        void ApplyFilmVariants(const film::Info& f, int (&characters)[2])
        {
            ClearFilmVariants("a new film");
            for (int& c : g_filmCharacterOverride) c = -1;
            for (int s = 0; s < 4; ++s) ClearVariant(s); // this PC's own pick, or one from a lobby, must not show in the film
            for (int s = 0; s < 4; ++s)
            {
                if (!f.present[s] && f.variant[s].empty()) continue;
                int carrierSlot = -1;
                bool carrier = IsCarrierValue(f.character[s], carrierSlot);
                if (!f.variant[s].empty())
                {
                    int slot = -1;
                    variants::Pick pick;
                    std::string why;
                    if (!ParseVariant(f.variant[s], slot, pick, why))
                    {
                        Log(L"theatre: slot %d's Advanced pick '%S' can't be used: %S", s, f.variant[s].c_str(), why.c_str());
                    }
                    else
                    {
                        if (slot != s) Log(L"theatre: slot %d's Advanced pick was shared for slot %d - played on slot %d", s, slot, s);
                        int value = ActivateVariant(s, pick);
                        Log(L"theatre: slot %d plays the Advanced pick '%S' -> carrier value %X (film's value %X)%s", s, f.variant[s].c_str(), value,
                            f.character[s], value < 0 ? L" - NOT ACTIVE" : L"");
                        if (value >= 0)
                        {
                            g_filmVariantSlots |= 1u << s;
                            PrebuildVariant(s, pick, "a Theatre film");
                            if (s < 2) characters[s] = value;
                            else g_filmCharacterOverride[s] = value;
                            continue;
                        }
                    }
                }
                // No pick recorded (a film from before this build) but a carrier value: without its variant the carrier
                // monster's own files load, which crashed. Belono (26) is also a normal monster pick, so only Mechano,
                // Euroto and Hedono are replaced, by Edgar (0).
                if (carrier && (f.character[s] & 0xffff) != 26)
                {
                    Log(L"theatre: slot %d has carrier value %X but no Advanced pick in the film (recorded before Experimental 186) - plays as Edgar", s, f.character[s]);
                    if (s < 2) characters[s] = 0;
                    g_filmCharacterOverride[s] = 0;
                }
                else if (carrier)
                    Log(L"theatre: slot %d has value %X (Belono) and no Advanced pick - played as Belono", s, f.character[s]);
            }
            LogPickTable("Theatre film");
        }

        void ClearFilmVariants(const char* why)
        {
            if (!g_filmVariantSlots) return;
            uint32_t slots = g_filmVariantSlots;
            g_filmVariantSlots = 0;
            for (int s = 0; s < 4; ++s)
                if (slots & (1u << s)) ClearVariant(s);
            Log(L"theatre: film variants off (%S)", why);
        }

        // A carrier file the game opens: built for the active variant (BuildVariantFile), or empty.
        std::wstring VariantFile(LPCWSTR name)
        {
            std::wstring path(name);
            for (wchar_t& ch : path) ch = ch == L'/' ? L'\\' : static_cast<wchar_t>(towlower(ch)); // the game writes "player\%s"
            size_t player = 0;
            for (size_t at = path.find(L"player\\"); at != std::wstring::npos; at = path.find(L"player\\", at + 1)) player = at;
            if (player == 0 && path.compare(0, 7, L"player\\") != 0) return L"";
            std::wstring rest = path.substr(player + 7); // <folder>\<file>
            size_t slash = rest.find(L'\\');
            if (slash == std::wstring::npos) return L"";
            std::string folder, file; // ASCII names: each wchar_t narrowed on purpose (no C4244 warning)
            for (size_t i = 0; i < rest.size(); ++i)
                if (i != slash) (i < slash ? folder : file).push_back(static_cast<char>(rest[i]));
            for (char& ch : file) ch = static_cast<char>(tolower(static_cast<unsigned char>(ch)));
            for (int slot = 0; slot < 4; ++slot)
            {
                variants::Pick pick;
                {
                    std::lock_guard guard(g_variantLock);
                    if (!g_variants[slot].on || _stricmp(folder.c_str(), kCarriers[slot].folder) != 0) continue;
                    pick = g_variants[slot].pick;
                }
                {
                    std::lock_guard guard(g_buildLock); // a prebuild may be reading it
                    std::wstring prefix(name, name + player); // up to Player\ (the game's own spelling)
                    if (prefix != g_dataPrefix) g_dataPrefix = prefix;
                }
                static ULONGLONG lastTable = 0; // the pick table once per match load
                if (GetTickCount64() - lastTable > 20000) { lastTable = GetTickCount64(); LogPickTable("the match loads a carrier's files"); }
                if (file.size() == 10 && file.compare(0, 5, "exmot") == 0 && file.compare(6, 4, ".ala") == 0 && isdigit(static_cast<unsigned char>(file[5])))
                    return BuildVariantFile(pick, file[5] - '0', name);
                if (file.size() == strlen(kCarriers[slot].folder) + 5 && file.compare(file.size() - 4, 4, ".alr") == 0 && isdigit(static_cast<unsigned char>(file[file.size() - 5])))
                    return BuildVariantFile(pick, -1, name);
            }
            return L"";
        }

        // variant_test=c,colour,dodge,set (numbers)
        bool VariantTestPick(variants::Pick& pick)
        {
            const std::string& text = GetConfig().variantTest;
            if (text.empty()) return false;
            int v[4] = { -1, 0, 0, 0 };
            if (sscanf_s(text.c_str(), "%d,%d,%d,%d", &v[0], &v[1], &v[2], &v[3]) < 1) return false;
            pick = { v[0], v[1], v[2], v[3] };
            return true;
        }

        HANDLE WINAPI HookedCreateFile2(LPCWSTR name, DWORD access, DWORD share, DWORD disposition, LPCREATEFILE2_EXTENDED_PARAMETERS params)
        {
            size_t n = name ? wcslen(name) : 0;
            if (g_anyRetry && n > 12 && (wcsstr(name, L"layer\\") || wcsstr(name, L"layer/"))) RetryMemberVariants("the match loads player files");
            bool anyVariant = false;
            for (const ActiveVariant& v : g_variants) anyVariant = anyVariant || v.on;
            if (anyVariant && n > 12)
            {
                static std::atomic<int> told{ 0 };
                if (told < 40 && (wcsstr(name, L"layer") || wcsstr(name, L".al"))) { ++told; Log(L"variants: the game opens %s (access %X)", name, access); }
            }
            if (anyVariant && n > 12 && (access & GENERIC_READ) && !(access & GENERIC_WRITE))
            {
                std::wstring built = VariantFile(name);
                if (!built.empty()) return g_createFile2(built.c_str(), access, share, disposition, params);
            }
            if (n > 16 && _wcsicmp(name + n - 15, L"menu\\FDLogo.alr") == 0 && (name[n - 16] == L'/' || name[n - 16] == L'\\')) // "Assets/Data/menu\FDLogo.alr"
            {
                static std::once_flag once;
                static std::wstring copy;
                std::call_once(once, [name]
                {
                    std::wstring path = LocalStatePath() + L"\\pdnet\\FDLogo.alr";
                    if (MakeTitleCopy(name, path) && TallerTitleBoxes()) copy = path;
                    Log(L"gamepatch: title screen %s", copy.empty() ? L"left as it is (logo file not as expected)" : L"reads \"Rebirth\"");
                });
                if (!copy.empty()) return g_createFile2(copy.c_str(), access, share, disposition, params);
            }
            if (n > 14 && _wcsicmp(name + n - 13, L"menu\\MNet.alr") == 0 && (name[n - 14] == L'/' || name[n - 14] == L'\\') && GetConfig().charselPortraits)
            {
                static std::once_flag once;
                static std::wstring copy;
                std::call_once(once, [name, n]
                {
                    // the game's data folder as it names it (for variant builds before any player file is opened)
                    g_dataPrefix = std::wstring(name, name + n - 13);
                    Log(L"variants: data folder '%s'", g_dataPrefix.c_str());
                    std::wstring path = LocalStatePath() + L"\\pdnet\\MNet.alr";
                    const char* why = "";
                    int added = MakeMnetCopy(name, path, why);
                    if (added >= 0) copy = path, g_monsterPortraits = added;
                    if (added >= 0) Log(L"gamepatch: character select portraits: %d sprites added (MNET sprites %d-%d: monsters, blank, Advanced list portraits)", added, kMonsterPortraitFirst, kMonsterPortraitFirst + added - 1);
                    else Log(L"gamepatch: character select portraits left as they are (%S)", why);
                });
                if (!copy.empty()) return g_createFile2(copy.c_str(), access, share, disposition, params);
            }
            // debug_menu=on (developer copies): the debug menu's script is menu 2's menu1_en.ssb; a copy that has it as
            // debug.ssb (with option_en.ssb overwritten by it, the real Options kept as option_enx.ssb) gets those
            // swapped back. Split Screen opens it (HookedPushMenu).
            if (GetConfig().debugMenu && n > 14)
            {
                std::wstring path(name);
                auto endsWith = [&](const wchar_t* tail) { size_t t = wcslen(tail); return path.size() > t && _wcsicmp(path.c_str() + path.size() - t, tail) == 0; };
                auto swapTail = [&](const wchar_t* tail, const wchar_t* with) { return path.substr(0, path.size() - wcslen(tail)) + with; };
                auto exists = [](const std::wstring& p) { return GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES; };
                std::wstring other;
                // The game's own menu1_en.ssb is in Japanese; Assets\Data\MenuD\menu1_en.ssb (the debug build's menus) has
                // the English texts, so that one is used where it exists.
                if (endsWith(L"menu\\menu1_en.ssb") && exists(swapTail(L"menu\\menu1_en.ssb", L"MenuD\\menu1_en.ssb")))
                    other = swapTail(L"menu\\menu1_en.ssb", L"MenuD\\menu1_en.ssb");
                else if (endsWith(L"\\menu1_en.ssb") && !exists(path)) other = swapTail(L"menu1_en.ssb", L"debug.ssb");
                else if (endsWith(L"\\option_en.ssb")) other = swapTail(L"option_en.ssb", L"option_enx.ssb");
                if (!other.empty() && exists(other))
                {
                    static std::atomic<int> told{ 0 };
                    if (told++ < 4) Log(L"gamepatch: debug menu: %s reads %s", path.c_str(), other.c_str());
                    return g_createFile2(other.c_str(), access, share, disposition, params);
                }
            }
            return g_createFile2(name, access, share, disposition, params);
        }
        // Returns false until Windows has filled in PDUWP.exe's imports (pdlaunch injects pdnet before that).
        // CreateFile2 in a module's import table -> HookedCreateFile2. PDUWP.exe's (the menus, FDLogo / MNet) and, for the
        // character files (test 6, 2026-11-20: none of them came through PDUWP.exe's), MeldRelease_UWP.dll's, the data
        // layer, which imports CreateFile2 too.
        bool PatchCreateFile2In(uint8_t* base)
        {
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + reinterpret_cast<IMAGE_DOS_HEADER*>(base)->e_lfanew);
            auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
            for (auto desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + dir.VirtualAddress); dir.VirtualAddress && desc->Name; ++desc)
            {
                if (!desc->OriginalFirstThunk) continue;
                auto names = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->OriginalFirstThunk);
                auto slots = reinterpret_cast<IMAGE_THUNK_DATA*>(base + desc->FirstThunk);
                for (; names->u1.AddressOfData; ++names, ++slots)
                {
                    if (IMAGE_SNAP_BY_ORDINAL(names->u1.Ordinal) ||
                        strcmp(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + names->u1.AddressOfData)->Name, "CreateFile2") != 0)
                        continue;
                    if (slots->u1.Function == names->u1.AddressOfData) return false; // not filled in yet
                    if (slots->u1.Function == reinterpret_cast<ULONG_PTR>(&HookedCreateFile2)) return true;
                    if (!g_createFile2) g_createFile2 = reinterpret_cast<CreateFile2Fn>(slots->u1.Function);
                    DWORD old;
                    VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), PAGE_READWRITE, &old);
                    slots->u1.Function = reinterpret_cast<ULONG_PTR>(&HookedCreateFile2);
                    VirtualProtect(&slots->u1.Function, sizeof(ULONG_PTR), old, &old);
                    return true;
                }
            }
            return false;
        }

        bool PatchCreateFile2()
        {
            if (!PatchCreateFile2In(Image())) return false;
            if (HMODULE meld = GetModuleHandleW(L"MeldRelease_UWP.dll"))
                Log(L"gamepatch: MeldRelease_UWP.dll file opens %s", PatchCreateFile2In(reinterpret_cast<uint8_t*>(meld)) ? L"hooked" : L"NOT hooked");
            else Log(L"gamepatch: MeldRelease_UWP.dll not loaded yet - its file opens not hooked");
            return true;
        }
    }

    namespace
    {
        // Lock onto partner: RB on a controller, or the partner_lock_key key. partner_aim=modern (Settings > Partner Aim):
        // it switches between the game's own target and this player's teammate. partner_aim=classic: it only switches the
        // game's own option, Options > Details > Lock Onto Partner (the byte below), which lets switch-target cycle through teammates.
        // A human player's target is the block at player object +0x13f10 (FUN_14024ec80 -> FUN_140252a20 each frame,
        // the call at 0x14024f120): +0x10 is the target, an index into the targetable objects DAT_1404bcb50 (32, each
        // with the player's status record at +0xb8), +0xc the distance band, +0x300 the player. FUN_1402531d0 inside
        // keeps a target only while FUN_1402543e0 accepts it, which refuses teammates unless the option byte
        // 0x14052dfb9 (Lock Onto Partner) is set. HookedTargetUpdate sets the partner as target and that byte for the
        // update, while partner lock is on. (Experimental 94-96 hooked FUN_1401775b0's call instead: that is the AI's
        // targeting, run only for computer players - it never ran for the player in an online match.)
        using TargetUpdateFn = void (*)(uint8_t* target);
        std::atomic<bool> g_partnerLock{ false };   // on: this player's lock target is the partner
        std::atomic<bool> g_partnerToggle{ false }; // a press waiting for the game thread

        bool InMatch()
        {
            int32_t state = *reinterpret_cast<volatile int32_t*>(At(0x1404bc950));
            return state != 0 && state != 13;
        }

        uint8_t* PlayerObject(int slot) { return slot >= 0 && slot < 4 ? reinterpret_cast<uint8_t**>(At(0x1404bc9d8))[slot] : nullptr; }
        uint8_t* PlayerRecord(int slot) { return slot >= 0 && slot < 4 ? reinterpret_cast<uint8_t**>(At(0x1404bc608))[slot] : nullptr; }

        // The other living player on this player's team (team = object +0x1c, 0 or 1; FUN_140187180), or -1.
        int PartnerSlot(int me)
        {
            uint8_t* mine = PlayerObject(me);
            if (!mine || *reinterpret_cast<int32_t*>(mine + 0x10) != 0) return -1;
            uint32_t team = *reinterpret_cast<uint32_t*>(mine + 0x1c);
            if (team >= 2) return -1;
            for (int s = 0; s < 4; ++s)
            {
                uint8_t* other = PlayerObject(s);
                uint8_t* record = PlayerRecord(s);
                if (s == me || !other || !record || *reinterpret_cast<int32_t*>(other + 0x10) != 0) continue;
                if (*reinterpret_cast<uint32_t*>(other + 0x1c) == team && *reinterpret_cast<int32_t*>(record + 0xc) > 0) return s;
            }
            return -1;
        }

        // The partner's index among the targetable objects (DAT_1404bcb50, whose +0xb8 is the player's status record), or -1.
        int PartnerTarget(int partner)
        {
            uint8_t* object = PlayerObject(partner);
            uint8_t* record = PlayerRecord(partner);
            uint8_t** targets = reinterpret_cast<uint8_t**>(At(0x1404bcb50));
            for (int i = 0; i < 32; ++i)
            {
                uint8_t* status = targets[i] ? *reinterpret_cast<uint8_t**>(targets[i] + 0xb8) : nullptr;
                if (status && (status == record || (object && status == object + 0x40))) return i;
            }
            return -1;
        }

        // The target block's fields (int offsets): +0x4 locked on, +0x10 target, +0x14 aiming at it this frame. The
        // player's pad bits (FUN_140256580): +0x1425c held, +0x14264 pressed this frame; 0x800 = lock on / off
        // (FUN_140252a20), 0x400 = switch target (FUN_1402531d0).
        void Unlock(uint8_t* target)
        {
            *reinterpret_cast<int32_t*>(target + 0x4) = 0;
            *reinterpret_cast<int32_t*>(target + 0x10) = -1;
            *reinterpret_cast<int32_t*>(target + 0x14) = 0;
        }

        // Enemies and the partner are kept apart: the game's own lock-on and switch-target only ever reach enemies (the
        // Lock Onto Partner option byte is held at 0 for this PC's player), and partner lock only the partner. While
        // locked onto the partner, the game's lock-on or switch-target button goes to an enemy.
        std::atomic<int> g_classicOption{ -1 }; // partner_aim=classic: the option byte in this match; -1: not read yet
        // Is the current target a teammate (alive or down)? Target object +0xb8 = the status record, +0x61a0 = its player.
        bool TargetIsTeammate(uint8_t* target, uint8_t* player)
        {
            int index = *reinterpret_cast<int32_t*>(target + 0x10);
            if (index < 0 || index >= 32) return false;
            uint8_t* object = reinterpret_cast<uint8_t**>(At(0x1404bcb50))[index];
            uint8_t* status = object ? *reinterpret_cast<uint8_t**>(object + 0xb8) : nullptr;
            uint8_t* owner = status ? *reinterpret_cast<uint8_t**>(status + 0x61a0) : nullptr;
            return owner && owner != player && *reinterpret_cast<uint32_t*>(owner + 0x1c) == *reinterpret_cast<uint32_t*>(player + 0x1c);
        }

        void HookedTargetUpdate(uint8_t* target)
        {
            auto original = reinterpret_cast<TargetUpdateFn>(At(0x140252a20));
            uint8_t* player = *reinterpret_cast<uint8_t**>(target + 0x300);
            int me = *reinterpret_cast<int32_t*>(At(0x1404bbe00));
            if (!player || player != PlayerObject(me)) return original(target);
            static std::atomic<bool> seen{ false };
            if (!seen.exchange(true)) Log(L"gamepatch: lock onto partner: target hook running for this PC's player %d", me);
            if (GetConfig().partnerClassic)
            {
                g_partnerLock = false;
                // Held here while the match runs, so the second press always turns it off, whatever else writes the byte.
                volatile uint8_t* option = At(0x14052dfb9);
                if (g_classicOption < 0) g_classicOption = *option ? 1 : 0;
                if (g_partnerToggle.exchange(false))
                {
                    g_classicOption = g_classicOption ? 0 : 1;
                    Log(L"gamepatch: lock onto partner (classic): the game's option is %s", g_classicOption ? L"on" : L"off");
                    if (!g_classicOption && TargetIsTeammate(target, player)) *reinterpret_cast<int32_t*>(target + 0x10) = -1; // back to an enemy
                }
                *option = g_classicOption ? 1 : 0;
                return original(target);
            }
            if (g_partnerToggle.exchange(false))
            {
                bool on = !g_partnerLock;
                if (on && PartnerSlot(me) < 0) Log(L"gamepatch: lock onto partner - no partner here");
                else
                {
                    g_partnerLock = on;
                    Log(L"gamepatch: lock onto partner %s", on ? L"on" : L"off");
                    if (!on) Unlock(target);
                }
            }
            uint32_t pressed = *reinterpret_cast<uint32_t*>(player + 0x14264);
            if (g_partnerLock && (pressed & 0xc00))
            {
                g_partnerLock = false;
                // Switch target: the game moves from "no target" to the first enemy. Lock on: with nothing aimed at,
                // the game locks onto an enemy instead of unlocking.
                *reinterpret_cast<int32_t*>(target + 0x10) = -1;
                *reinterpret_cast<int32_t*>(target + 0x14) = 0;
                Log(L"gamepatch: lock onto partner off - %s", pressed & 0x400 ? L"switch target to an enemy" : L"lock onto an enemy");
            }
            int index = -1;
            if (g_partnerLock)
            {
                int partner = PartnerSlot(me);
                index = partner < 0 ? -1 : PartnerTarget(partner);
                if (index < 0)
                {
                    g_partnerLock = false;
                    Unlock(target);
                    Log(L"gamepatch: lock onto partner off - the partner is gone or down");
                }
                else
                {
                    *reinterpret_cast<int32_t*>(target + 0x4) = 1; // locked on, as the game's own lock-on does
                    *reinterpret_cast<int32_t*>(target + 0x10) = index;
                }
            }
            volatile uint8_t* allowPartner = At(0x14052dfb9);
            uint8_t saved = *allowPartner;
            *allowPartner = index >= 0 ? 1 : 0; // the partner only through partner lock
            original(target);
            *allowPartner = saved;
            if (index >= 0) *reinterpret_cast<int32_t*>(target + 0x10) = index;
        }

        // The scoreboard battle's start, for Test Arsenal's map and character (see g_testPicking).
        using MatchSetupFn = void (*)(uint64_t a, uint32_t localSlot, int mode, uint8_t* setup, uint32_t e, int stage, uint32_t layout);
        void HookedTestStart(uint64_t a, uint32_t localSlot, int mode, uint8_t* setup, uint32_t e, int stage, uint32_t layout)
        {
            if (g_testBattle)
            {
                if (g_testStage >= 0) { stage = g_testStage; layout = g_testLayout; }
                if (g_testCharacter >= 0) *reinterpret_cast<int32_t*>(setup + 8) = g_testCharacter;
                variants::Pick test;
                if (VariantTestPick(test))
                {
                    int value = ActivateVariant(0, test);
                    if (value >= 0) *reinterpret_cast<int32_t*>(setup + 8) = value;
                }
                if (g_testOpponentCharacter >= 0) *reinterpret_cast<int32_t*>(setup + 0x48 + 8) = g_testOpponentCharacter;
                Log(L"gamepatch: Test Arsenal: battle on map %d, layout %u, character %d, opponent's %d", stage, layout, *reinterpret_cast<int32_t*>(setup + 8), *reinterpret_cast<int32_t*>(setup + 0x48 + 8));
                // A film with 3 or 4 players: the setup has 4 entries of 0x48 (FUN_140128990 copies them; a player is
                // there when its first int isn't 0) and this battle fills 2. The others are copies of the opponent's
                // entry with the film player's character, and their deck counts are built from the film (slots 0 and 1
                // get theirs through HookedDeck).
                int players = labtests::FilmPlayers();
                for (int k = 2; k < players && k < 4; ++k)
                {
                    memcpy(setup + k * 0x48, setup + 0x48, 0x48);
                    int character = g_filmCharacterOverride[k] >= 0 ? g_filmCharacterOverride[k] : labtests::FilmCharacter(k);
                    if (character >= 0) *reinterpret_cast<int32_t*>(setup + k * 0x48 + 8) = character;
                    int16_t deck[30];
                    for (int16_t& id : deck) id = -1;
                    labtests::FilmDeck(k, deck);
                    reinterpret_cast<float*>(At(0x1404bc958))[k] = 1.0f; // its Aura rate
                    if (reinterpret_cast<uint8_t**>(At(0x140585fe8))[k]) reinterpret_cast<DeckFn>(At(0x14014f830))(0, k, deck); // its deck counts, when allocated
                    Log(L"theatre: battle slot %d added for the film (character %d, deck %d %d %d ...)", k, *reinterpret_cast<int32_t*>(setup + k * 0x48 + 8), deck[0], deck[1], deck[2]);
                }
            }
            reinterpret_cast<MatchSetupFn>(At(0x140128990))(a, localSlot, mode, setup, e, stage, layout);
        }

        void TestArsenalChoices()
        {
            uint8_t* site = At(0x140214a09);
            uint8_t* cave = CodeCave(16);
            if (site[0] != 0xe8 || site + 5 + *reinterpret_cast<int32_t*>(site + 1) != At(0x140128990) || !cave)
            {
                Log(L"gamepatch: Test Arsenal map and character skipped - the game code is not what this pdnet expects");
                return;
            }
            std::vector<uint8_t> toHook = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 }; // jmp [rip+0]
            Put64(toHook, reinterpret_cast<uint64_t>(&HookedTestStart));
            std::vector<uint8_t> call = { 0xe8 };
            Put32(call, Rel32(site + 5, cave));
            if (!Write(cave, toHook) || !Write(site, call)) Log(L"gamepatch: Test Arsenal map and character failed (VirtualProtect error %lu)", GetLastError());
        }

        // Presses: RB (GamepadButtons RightShoulder) on any controller, polled; the key from the game's window.
        void PartnerPress(const wchar_t* source)
        {
            bool match = InMatch();
            Log(L"gamepatch: lock onto partner: %s pressed%s", source, match ? L"" : L" (not in a match: ignored)");
            if (match) g_partnerToggle = true;
        }

        // Options > Keyboard: one more row after the game's 22 (FUN_1400be7f0, g10 #18, row and column: 0 the name, 1 the key, 2
        // the second key). A pick is g10 #19 (the row goes to 0x1403dd3d8, the script waits for -1). The game's key handler
        // does nothing for a row past its table, so the key press is taken in HookedKey and saved as partner_lock_key.
        constexpr int kPartnerKeyRow = 0x16;
        NativeFn g_keyRow = nullptr, g_keyDefaults = nullptr;
        int64_t HookedKeyRow(int64_t* args)
        {
            if (!args || args[0] < kPartnerKeyRow) return g_keyRow(args);
            if (args[0] > kPartnerKeyRow) return 0;
            if (args[1] == 0) return reinterpret_cast<int64_t>("Lock Onto Partner");
            if (args[1] != 1) return 0;
            if (*reinterpret_cast<int32_t*>(At(0x1403dd3d8)) == kPartnerKeyRow) return reinterpret_cast<int64_t>("PRESS A KEY");
            uint32_t key = GetConfig().partnerLockKey;
            uint8_t* names = At(0x1403dd470); // 0x97 entries: key code, then its name at +8
            for (int i = 0; i < 0x97; ++i)
                if (*reinterpret_cast<uint32_t*>(names + i * 0x10) == key) return *reinterpret_cast<int64_t*>(names + i * 0x10 + 8);
            return reinterpret_cast<int64_t>("?");
        }

        void SavePartnerKey(uint32_t key)
        {
            EditableConfig().partnerLockKey = key;
            // In a form KeyCode reads back: one digit alone would mean that key, not that code.
            SaveSetting("partner_lock_key", key == 0x09 ? "Tab" : key == 0x08 ? "Backspace" : std::to_string(key));
            Log(L"gamepatch: lock onto partner: keyboard key is now 0x%02X", key);
        }

        int64_t HookedKeyDefaults(int64_t* args)
        {
            int64_t result = g_keyDefaults(args);
            if (GetConfig().partnerLockKey != 0x09) SavePartnerKey(0x09);
            return result;
        }

        // Keys come from the game's own key-down handler, FUN_140008490 (this, sender, KeyEventArgs): pdnet's subscriptions to
        // CoreWindow KeyDown (Experimental 94-120) and the dispatcher's AcceleratorKeyActivated (121) never reported a key.
        void HookedKey(uint32_t key)
        {
            int32_t* assigning = reinterpret_cast<int32_t*>(At(0x1403dd3d8));
            if (*assigning == kPartnerKeyRow)
            {
                if (key != 0x0D && key != 0x1B) SavePartnerKey(key); // Enter and Esc cancel, as for the game's own rows
                *assigning = -1;
                return;
            }
            if (key && key == GetConfig().partnerLockKey) PartnerPress(L"key");
            if (CurrentMenu(MenuManager()) == kCharacterMenu) // the Advanced list (CharacterSelectInput / CharacterSelectButtons)
            {
                if (key == 'Y') g_charselKeyY = true;
                if (key == VK_UP || key == 'W') g_charselKeyMove = 1;
                if (key == VK_DOWN || key == 'S') g_charselKeyMove = 2;
                if (key == VK_LEFT || key == 'A') g_charselKeyMove = 3;
                if (key == VK_RIGHT || key == 'D') g_charselKeyMove = 4;
                if (key == 'Q') g_charselKeyPage = -1;
                if (key == 'E') g_charselKeyPage = 1;
            }
        }

        // KeyEventArgs: get_VirtualKey at vtable +0x30, get_KeyStatus at +0x38 (CorePhysicalKeyStatus: WasKeyDown at byte 10).
        using GameKeyFn = int64_t (*)(void* self, void* sender, void** args);
        GameKeyFn g_gameKey = nullptr;
        // On some PCs (the developer's) the game's window gets characters but no key-down or key-up events at all, so every
        // keyboard control is dead (no 'handler ran' in 123's log with a key row waiting; why isn't known). pdlaunch reads the
        // keyboard and KeyboardBridge hands the changes to the game's own handlers (FUN_140008490 down, FUN_140008600 up), as
        // these stand-in KeyEventArgs. It stops for good once the window delivers a real key event, so nothing comes twice.
        std::atomic<bool> g_windowKeys{ false };
        struct FakeKeyArgs
        {
            void** vtable;
            static HRESULT __stdcall Zero(void*) { return S_OK; }
            static HRESULT __stdcall Key(void*, int32_t* key) { *key = Current(); return S_OK; }
            static HRESULT __stdcall Status(void*, uint8_t* status) { memset(status, 0, 12); return S_OK; }
            static int32_t& Current() { static int32_t key = 0; return key; }
            void** self() { return reinterpret_cast<void**>(this); }
        };
        void* g_fakeKeyTable[8] = { reinterpret_cast<void*>(&FakeKeyArgs::Zero), reinterpret_cast<void*>(&FakeKeyArgs::Zero), reinterpret_cast<void*>(&FakeKeyArgs::Zero),
                                    reinterpret_cast<void*>(&FakeKeyArgs::Zero), reinterpret_cast<void*>(&FakeKeyArgs::Zero), reinterpret_cast<void*>(&FakeKeyArgs::Zero),
                                    reinterpret_cast<void*>(&FakeKeyArgs::Key), reinterpret_cast<void*>(&FakeKeyArgs::Status) };
        FakeKeyArgs g_fakeKeyArgs{ g_fakeKeyTable };
        int64_t HookedGameKey(void* self, void* sender, void** args)
        {
            if (args != g_fakeKeyArgs.self()) g_windowKeys = true;
            // Keys the game must not see (the user, 2026-11-16): while a name is typed in Online Settings (its letters
            // went to the menu as well) and while a Theatre film is paused (its own controls are pdnet's).
            if ((g_typingName && InSettings()) || labtests::FilmPaused()) return 0;
            int64_t result = g_gameKey(self, sender, args);
            if (!args) return result;
            void** vtable = reinterpret_cast<void**>(*args);
            int32_t key = 0;
            uint8_t status[12] = {};
            auto read = reinterpret_cast<HRESULT (__stdcall*)(void*, void*)>(vtable[6]);
            auto readStatus = reinterpret_cast<HRESULT (__stdcall*)(void*, void*)>(vtable[7]);
            if (SUCCEEDED(read(args, &key)) && SUCCEEDED(readStatus(args, status)) && !status[10]) HookedKey(static_cast<uint32_t>(key));
            return result;
        }

        void KeyboardBridge()
        {
            std::thread([]
            {
                uint8_t held[256] = {}, now[256];
                bool told = false;
                for (;;)
                {
                    Sleep(8);
                    if (g_windowKeys)
                    {
                        Log(L"gamepatch: keyboard: the window delivers keys on this PC - pdlaunch's aren't needed");
                        return;
                    }
                    void* app = *reinterpret_cast<void**>(At(0x1405ae170));
                    if (!app || !g_gameKey) continue;
                    if (!KeysHeld(now) || g_typingName || labtests::FilmPaused()) memset(now, 0, sizeof(now)); // typing a name: its characters are enough; a paused film: its own controls
                    for (int key = 0; key < 256; ++key)
                    {
                        if (now[key] == held[key]) continue;
                        held[key] = now[key];
                        FakeKeyArgs::Current() = key;
                        if (now[key]) HookedGameKey(app, nullptr, g_fakeKeyArgs.self());
                        else reinterpret_cast<GameKeyFn>(At(0x140008600))(app, nullptr, g_fakeKeyArgs.self());
                        if (!told)
                        {
                            told = true;
                            Log(L"gamepatch: keyboard: keys from pdlaunch go to the game's handlers (first 0x%02X)", key);
                        }
                    }
                }
            }).detach();
        }

        void PartnerLock()
        {
            uint8_t* site = At(0x14024f120);
            uint8_t* cave = CodeCave(16);
            if (site[0] != 0xe8 || site + 5 + *reinterpret_cast<int32_t*>(site + 1) != At(0x140252a20) || !cave)
            {
                Log(L"gamepatch: lock onto partner skipped - the game code is not what this pdnet expects");
                return;
            }
            std::vector<uint8_t> toHook = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 }; // jmp [rip+0]
            Put64(toHook, reinterpret_cast<uint64_t>(&HookedTargetUpdate));
            std::vector<uint8_t> call = { 0xe8 };
            Put32(call, Rel32(site + 5, cave));
            if (!Write(cave, toHook) || !Write(site, call))
            {
                Log(L"gamepatch: lock onto partner failed (VirtualProtect error %lu)", GetLastError());
                return;
            }
            std::thread([]
            {
                bool held = false;
                for (;;)
                {
                    Sleep(16);
                    if (!InMatch()) { g_partnerLock = false; g_partnerToggle = false; g_classicOption = -1; held = false; continue; }
                    bool now = AnyPadHolds(0x800); // RightShoulder
                    if (now && !held) PartnerPress(L"RB");
                    held = now;
                }
            }).detach();
            uint32_t key = GetConfig().partnerLockKey;
            Log(L"gamepatch: lock onto partner ready (RB%s)", key ? L" or the partner_lock_key key, once the menus run" : L"");
            std::vector<uint8_t> start = { 0x48, 0x89, 0x5C, 0x24, 0x08, 0x56, 0x48, 0x83, 0xEC, 0x40 }; // mov [rsp+8],rbx / push rsi / sub rsp,40h
            g_gameKey = reinterpret_cast<GameKeyFn>(InlineHookBytes(0x140008490, start, start, reinterpret_cast<void*>(&HookedGameKey), L"keyboard key"));
            Log(L"gamepatch: lock onto partner: keyboard key 0x%02X (%s)", key, g_gameKey ? L"hooked" : L"NOT hooked");
            if (g_gameKey) KeyboardBridge();
            bool row = WrapCommand<NativeFn>(0x1403ddde0 + 18 * 8, 0x1400be7f0, &HookedKeyRow, g_keyRow, L"partner lock key row") &&
                       WrapCommand<NativeFn>(0x1403ddde0 + 20 * 8, 0x1400be990, &HookedKeyDefaults, g_keyDefaults, L"partner lock key default");
            Log(L"gamepatch: Options > Keyboard: Lock Onto Partner row %s", row ? L"ready" : L"NOT added");
        }
    }

    namespace
    {
        // Modpacks (PD Helper's ModLoader = Skill Editor) write their skill data straight into the game's copy of
        // gsdata_<lang>.dat in memory: the block at 0x1404c5240 (pointer DAT_1404c5228, 0x44004 bytes). The game fills it
        // from the file at start-up (FUN_14017da90, done flag DAT_140509b90), then once, on the first main menu or session
        // join (g10 #0 = FUN_1400bd660), copies its stored online copy over it (buffer DAT_1403dd458, size DAT_1403dd460, done
        // flag DAT_1403dd465; from the save's UserConfig\gsdata_<lang>.dat). So what the game runs on isn't the file: 2627
        // bytes apart on the developer's PC with no mod, which is why Experimental 97-108 saw every player as modded and took
        // a revert for an edit, without a word in the log. Players saw mods go back to vanilla after matches, online / lobby
        // screens and the title screen; that one copy is the only game code found that could do it, so the cause isn't
        // pinned down. SkillDataGuard keeps the mod: every 200 ms it compares the block with the data as the game loaded it
        // and the last modded state. Changes that aren't a return to the game's data are the new modded state (Skill
        // Editor's live edits too); when most modded bytes return to it at once, they are put back and the moment is logged.
        // Each match start logs how many bytes are modded, to tell a change in the block from anything else.
        constexpr size_t kSkillDataSize = 0x44004;

        std::vector<uint8_t> SkillDataFile()
        {
            wchar_t exe[MAX_PATH];
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::wstring dir = exe;
            dir = dir.substr(0, dir.find_last_of(L'\\') + 1);
            bool japanese = *reinterpret_cast<int32_t*>(At(0x1403df644)) == 2;
            std::wstring path = dir + L"Assets\\Data\\gstorage\\" + (japanese ? L"gsdata_ja.dat" : L"gsdata_en.dat");
            std::vector<uint8_t> data;
            if (FILE* f = nullptr; !_wfopen_s(&f, path.c_str(), L"rb") && f)
            {
                data.resize(kSkillDataSize);
                data.resize(fread(data.data(), 1, data.size(), f));
                fclose(f);
            }
            if (data.size() != kSkillDataSize) data.clear();
            return data;
        }

        // The game's stored online copy, if it's there and whole (the sign-in code can free it: hence __try).
        bool CopyStoredSkillData(uint8_t* to)
        {
            __try
            {
                const uint8_t* stored = *reinterpret_cast<uint8_t**>(At(0x1403dd458));
                if (!stored || *reinterpret_cast<int32_t*>(At(0x1403dd460)) != kSkillDataSize || *reinterpret_cast<const int32_t*>(stored) != kSkillDataSize) return false;
                memcpy(to, stored, kSkillDataSize);
                return true;
            }
            __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
        }

        // Skill data from a mod (Conceptual, mod loader step 6). PDPL hands the game a mod's gsdata_<lang>.dat at start-up,
        // but the stored online copy then goes over it (see above), so the mod is lost. pdlaunch puts the gsdata file the
        // game would get from the mods in LocalState\pdnet\mod-skills\ at every start (launcher\modsync.cpp), and
        // SkillDataGuard writes it into the block once the stored copy is in: only the bytes where the mod's file differs
        // from the game folder's, so values the mod didn't change keep the stored copy's (Skill Update 1.40) numbers. The
        // result is the guard's modded state, so it's kept like a modpack. Only while PDPL is in the game: without it
        // (mods=off, safe mode) a file left from an earlier start must not apply.
        std::vector<uint8_t> ModSkillData(std::string& from)
        {
            std::vector<uint8_t> data;
            if (!GetModuleHandleW(L"pd_loader_core.dll")) return data;
            bool japanese = *reinterpret_cast<int32_t*>(At(0x1403df644)) == 2;
            std::wstring base = LocalStatePath() + L"\\pdnet\\mod-skills\\" + (japanese ? L"gsdata_ja" : L"gsdata_en");
            if (FILE* f = nullptr; !_wfopen_s(&f, (base + L".dat").c_str(), L"rb") && f)
            {
                data.resize(kSkillDataSize + 1);
                data.resize(fread(data.data(), 1, data.size(), f));
                fclose(f);
            }
            if (data.empty()) return data;
            if (data.size() != kSkillDataSize)
            {
                Log(L"gamepatch: skill data from mods: %s.dat is %zu bytes, not %zu - not used", base.c_str(), data.size(), kSkillDataSize);
                data.clear();
                return data;
            }
            std::ifstream in(base + L".txt");
            std::getline(in, from);
            return data;
        }

        void SkillDataGuard()
        {
            std::thread([]
            {
                uint8_t* block = At(0x1404c5240);
                std::vector<uint8_t> file = SkillDataFile(), stock, modded, now(kSkillDataSize);
                bool stockHasStored = false; // the stored copy is in `stock`
                bool modTried = false;       // a mod's skill data has been looked for (once the stored copy is in)
                int lastMenu = -1;
                for (;;)
                {
                    Sleep(200);
                    if (!*reinterpret_cast<volatile int32_t*>(At(0x140509b90))) continue; // still loading the file
                    memcpy(now.data(), block, kSkillDataSize);
                    bool applied = *reinterpret_cast<volatile uint8_t*>(At(0x1403dd465)) != 0;
                    if (modded.empty() && (stock.empty() || (applied && !stockHasStored)))
                    {
                        stock = now; // the game's own data, as far as it has got
                        stockHasStored = applied;
                        Log(L"gamepatch: modpack guard: skill data loaded (%s)", applied ? L"the stored copy" : L"the file");
                        continue;
                    }
                    if (!modded.empty() && applied && !stockHasStored)
                    {
                        // The game's one copy landed on a modpack loaded before it (the first main menu or join).
                        if (CopyStoredSkillData(stock.data())) Log(L"gamepatch: modpack guard: the game applied its stored copy over the modpack");
                        stockHasStored = true;
                    }
                    if (!modTried && applied && stockHasStored)
                    {
                        modTried = true;
                        std::string from;
                        std::vector<uint8_t> mod = ModSkillData(from);
                        if (!mod.empty())
                        {
                            // The mod's own changes: where it differs from the game folder's file. If that file couldn't be
                            // read, or reads the same as the mod (it would if PDPL handed pdnet the mod's), the whole file.
                            bool diff = !file.empty() && memcmp(file.data(), mod.data(), kSkillDataSize) != 0;
                            size_t written = 0;
                            for (size_t i = 0; i < kSkillDataSize; ++i)
                                if ((!diff || mod[i] != file[i]) && block[i] != mod[i]) { block[i] = mod[i]; ++written; }
                            memcpy(now.data(), block, kSkillDataSize);
                            modded = now;
                            Log(L"gamepatch: skill data from mod %S: %zu bytes written over the stored copy (%s)", from.c_str(), written,
                                diff ? L"the mod's changes to the game's file" : L"the whole file");
                            continue;
                        }
                    }
                    if (modded.empty())
                    {
                        if (memcmp(now.data(), stock.data(), kSkillDataSize) == 0) continue;
                        modded = now;
                        size_t bytes = 0;
                        for (size_t i = 0; i < kSkillDataSize; ++i) bytes += modded[i] != stock[i];
                        Log(L"gamepatch: modpack loaded (%zu bytes) - its skill data will be kept until the game closes", bytes);
                        continue;
                    }
                    int menu = CurrentMenu(MenuManager());
                    if (menu != lastMenu && menu == 1)
                    {
                        size_t bytes = 0;
                        for (size_t i = 0; i < kSkillDataSize; ++i) bytes += now[i] != stock[i];
                        Log(L"gamepatch: modpack guard: match starting, %zu bytes of skill data are modded", bytes);
                    }
                    lastMenu = menu;
                    if (memcmp(now.data(), modded.data(), kSkillDataSize) == 0) continue;
                    // Bytes back at the game's data (or the file's), how many of them the mod had changed; and anything new?
                    auto back = [&](size_t i) { return now[i] != modded[i] && (now[i] == stock[i] || (!file.empty() && now[i] == file[i])); };
                    size_t moddedBytes = 0, reverted = 0, otherChanges = 0;
                    for (size_t i = 0; i < kSkillDataSize; ++i)
                    {
                        bool isMod = modded[i] != stock[i];
                        if (isMod) ++moddedBytes;
                        if (back(i)) { if (isMod) ++reverted; }
                        else if (now[i] != modded[i]) ++otherChanges;
                    }
                    // A reload: many modded bytes (or most of them) back at once, little else new. A Skill Editor edit that
                    // sets one value back touches a few bytes and stays an edit.
                    if (reverted && (reverted >= 64 || reverted * 2 >= moddedBytes) && otherChanges * 10 <= reverted)
                    {
                        for (size_t i = 0; i < kSkillDataSize; ++i) // only what reverted: anything else the game wrote stays
                        {
                            if (back(i)) block[i] = modded[i];
                            else modded[i] = now[i];
                        }
                        Log(L"gamepatch: skill data went back to vanilla (%zu of %zu modded bytes; menu %d, game state %d) - modpack put back",
                            reverted, moddedBytes, menu, *reinterpret_cast<int32_t*>(At(0x1404bc950)));
                    }
                    else
                    {
                        static int told = 0;
                        if (told++ < 20) Log(L"gamepatch: modpack guard: skill data edited (%zu bytes back, %zu others; menu %d, game state %d)",
                                             reverted, otherChanges, menu, *reinterpret_cast<int32_t*>(At(0x1404bc950)));
                        modded = now; // an edit (Skill Editor) or another modpack: the new modded state
                    }
                }
            }).detach();
        }
    }

    namespace
    {
        // Main menu > HOW TO PLAY: the main menu script (mainmenu_en.ssb) shows it only when the title options have a
        // "how_to_play_url" (g10 #1, FUN_1400bd6a0: mode 0 checks, mode 1 opens it through Windows.System.Launcher).
        // Those options came from Xbox Live title storage (tms_options.csv, now the copy in the save), and Microsoft's
        // copy has only help_url, so the item was hidden. FUN_1400c2d80(key, default) looks a key up: it answers this
        // one with the community site.
        constexpr char kHowToPlayUrl[] = "https://phantomdust.online";
        using OptionFn = uint64_t (*)(const char* key, uint64_t fallback);
        OptionFn g_option = nullptr;

        uint64_t HookedOption(const char* key, uint64_t fallback)
        {
            if (key && _stricmp(key, "how_to_play_url") == 0)
            {
                static bool told = false;
                if (!told) { told = true; Log(L"gamepatch: main menu How to Play -> %S", kHowToPlayUrl); }
                return reinterpret_cast<uint64_t>(kHowToPlayUrl);
            }
            return g_option(key, fallback);
        }

        void HowToPlay()
        {
            // mov [rsp+8],rbx; mov [rsp+10h],rsi; push rdi; sub rsp,20h
            const std::vector<uint8_t> start = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x74, 0x24, 0x10, 0x57, 0x48, 0x83, 0xec, 0x20 };
            g_option = reinterpret_cast<OptionFn>(InlineHookBytes(0x1400c2d80, start, start, reinterpret_cast<void*>(&HookedOption), L"How to Play link"));
        }
    }

    namespace
    {
        // The skill preview (the small 3D view of what a skill does) in the multiplayer Arsenal editor and View Stock List.
        // In the story it is part of the town: the Lab is a menu over the running town session (game state 6), and the
        // preview borrows the town's player object (0x1404bc9d8) and is stepped by the town's frame (FUN_14012b110 calls
        // the table at 0x1402dd1a0 for the preview state 0x140534798: 0 idle, 1 set up, 2 load prevcam.dat, 3 running,
        // 4 stop, 5 finish). The multiplayer menus have no session (game state 0, no player), so:
        // 1. g5 #119 (the scripts' "preview on" check: bit 16 of the Lab's open parameter, set only by the story) is 1 in
        //    the editor (menu 64) and View Stock List (63), from the multiplayer menu and from the lobby (where this
        //    player's red hand stays on until the session has closed: LobbyAvailableAfterPreview).
        // 2. The preview's start (menu command 70 with 102) with no player starts the story's town session underneath
        //    (FUN_140212720, g4 #461 as townstart_en.ssb uses it: stage 9, the player's character) and is held until the
        //    town runs (game state 6 with a player object); then PreviewTick starts it, and the town's frame steps it.
        //    The script meanwhile waits on 103 (state 3) as it does in the story.
        // 3. The session stays for the whole visit to the Lab (one load per visit). Once the player has left the Lab's menus
        //    (InLabMenus) and the preview is idle, it ends the way g4 #458 does for a town (FUN_1402117e0's mode-0 path:
        //    FUN_14012b4f0). With the session's "menu after" left at -1 the game ends in menu state 13 without opening
        //    another menu (FUN_14012b7a0). A preview asked for while that ending runs (game states 9-12) is held and the
        //    session started again afterwards (the first version ended it at every stop, and going straight back into
        //    the editor hung on it).
        MenuCommandFn g_preview = nullptr;
        SessionCommandFn g_labPreviewMode = nullptr;
        bool g_previewTown = false;        // pdnet's town session is loading or running
        bool g_previewTownClosing = false; // pdnet ended it and the game hasn't finished ending it yet
        bool g_previewTownAgain = false;   // a preview came while it was ending: start the session again after
        int g_previewHeld = 0;             // 102 or 104 held until the town runs (0: none)
        bool g_previewHeldSkill = false;   // a 104 (show this skill) also came while held

        bool g_lobbyAvailableWhenClosed = false; // back in the lobby while the session was still there

        bool MenuGameState(int gameState) { return gameState == 0 || gameState == 13; }

        // The music: the Laboratory plays track 29 (29_DeckEdit.stx, g1 #16 -> FUN_14019cdf0); the current track is at
        // 0x14052acec. A hum started with pdnet's town session and stayed in the Lab until the main menus played
        // another track. While the session runs and after it, a different track than the Lab's is switched back to it.
        int g_previewTrack = -1; // the menu's track when pdnet's session started

        void KeepPreviewTrack(const wchar_t* when)
        {
            if (g_previewTrack < 0) return;
            int now = *reinterpret_cast<uint16_t*>(At(0x14052acec));
            if (now == g_previewTrack) return;
            Log(L"preview: %s the music changed to track %d - back to track %d", when, now, g_previewTrack);
            reinterpret_cast<int64_t (*)(uint32_t, int)>(At(0x14019cdf0))(static_cast<uint32_t>(g_previewTrack), 0);
        }

        // The flash before the editor: entering a town area (FUN_1401aefe0) sets the area banner bits 0x18000 at
        // 0x140530034 (`or edx, 0x18000` at 0x1401af6dd); FUN_1401c70f0 then draws the area's map sprite and name for a
        // moment. pdnet cleared them a frame late, so while its session runs that instruction ORs 0 instead.
        void PreviewAreaBanner(bool on)
        {
            uint8_t* immediate = At(0x1401af6df); // the 4-byte value of `81 CA 00 80 01 00`
            if (At(0x1401af6dd)[0] != 0x81 || At(0x1401af6dd)[1] != 0xca) return;
            Write(immediate, on ? std::vector<uint8_t>{ 0x00, 0x80, 0x01, 0x00 } : std::vector<uint8_t>{ 0x00, 0x00, 0x00, 0x00 });
        }

        bool g_previewZoneSaved = false;   // the save's zone and zone 0's area, while pdnet's session uses zone 0 / area 2
        int32_t g_previewZoneBefore = 0;
        uint8_t g_previewAreaBefore = 0;

        // The town load leaves its audio behind: FUN_14021a6a0 (from the stage load) loads the stage's ambience bank
        // (MapSEUnderground.bin) and DSP profile (DSP_Underground.bin; table 0x1402dc970 by stage % 10, current at
        // 0x1403e05f4), and only the next stage load replaces them, so a hum stayed in the menus. After pdnet's session
        // has closed, the menu profile (DSP_Menu.bin, class 0, no ambience bank) is loaded the same way: stage 0 for
        // the call, then FUN_14021ad70 starts the queued load, and the stage number is put back.
        void RestoreMenuAudio()
        {
            auto stage = reinterpret_cast<int32_t*>(At(0x140530264));
            int32_t stageBefore = *stage, dspBefore = *reinterpret_cast<int32_t*>(At(0x1403e05f4));
            *stage = 0;
            reinterpret_cast<void (*)()>(At(0x14021a6a0))();
            int64_t started = reinterpret_cast<int64_t (*)()>(At(0x14021ad70))();
            *stage = stageBefore;
            Log(L"preview: menu audio profile back (DSP class %d -> %d, loader %s)", dspBefore, *reinterpret_cast<int32_t*>(At(0x1403e05f4)),
                started ? L"started" : L"busy");
        }

        void RestorePreviewZone()
        {
            if (!g_previewZoneSaved) return;
            *reinterpret_cast<int32_t*>(At(0x14059a9dc)) = g_previewZoneBefore;
            *At(0x140599140) = g_previewAreaBefore;
            g_previewZoneSaved = false;
            Log(L"preview: zone %d and its area put back", g_previewZoneBefore);
        }

        void LobbyAvailableAfterPreview()
        {
            if (g_previewTown || g_previewTownClosing)
            {
                g_lobbyAvailableWhenClosed = true;
                Log(L"preview: back in the lobby - red hand stays on until the town session has closed");
                return;
            }
            SetLobbyAvailable(true);
        }

        void StartPreviewTown(int gameState)
        {
            // Stage 9 also loads one town area, st09_0N, and that area's scripts for the current zone
            // (st09_areaNN_ZZ): FUN_1401d85f0 takes N from the zone table (0x140599140 + zone * 0x14, byte 0) for the
            // zone at 0x14059a9dc, else 7. The online menus have the save's zone (13 on the developer PC: area 8), and
            // that area has no preview room. The story's Lab (read live, 2026-11-07) runs in zone 0, area 2, so
            // pdnet's session uses that pair while the stage loads (synchronously, inside the start) and puts both back.
            // preview_area=N (testing) takes area N with zone 0; -1 keeps the save's own.
            // (Other pairs crashed: an area with a zone it has no scripts for, e.g. area 2 with zone 13.)
            // The rest of the load reads the zone again over the next frames (putting it back right after the start
            // crashed the same way), so the pair stays until the session has closed (RestorePreviewZone).
            int area = GetConfig().previewArea == 0 ? 2 : GetConfig().previewArea;
            auto zonePtr = reinterpret_cast<int32_t*>(At(0x14059a9dc));
            uint8_t* zoneArea = At(0x140599140); // zone 0's entry
            bool patched = area >= 1 && area <= 8;
            if (patched && !g_previewZoneSaved)
            {
                g_previewZoneBefore = *zonePtr;
                g_previewAreaBefore = *zoneArea;
                g_previewZoneSaved = true;
            }
            if (patched)
            {
                *zonePtr = 0;
                *zoneArea = static_cast<uint8_t>(area);
            }
            if (g_previewTrack < 0) g_previewTrack = *reinterpret_cast<uint16_t*>(At(0x14052acec));
            PreviewAreaBanner(false);
            Log(L"preview: no player in this menu - starting the town session under it (game state %d, zone %d -> %d, area %d, music track %d)", gameState,
                g_previewZoneSaved ? g_previewZoneBefore : *zonePtr, *zonePtr, patched ? area : -1, g_previewTrack);
            reinterpret_cast<void (*)()>(At(0x140212720))();
            g_previewTown = true;
        }

        bool PreviewSessionOn() { return g_previewTown || g_previewTownClosing; }
        std::atomic<ULONGLONG> g_previewClosedAt{ 0 }; // when pdnet's last town session closed (for the lobby-leave log)
        ULONGLONG PreviewClosedAt() { return g_previewClosedAt; }

        bool PreviewPlayerReady()
        {
            return *reinterpret_cast<int32_t*>(At(0x1404bc950)) == 6 && *reinterpret_cast<uint64_t*>(At(0x1404bc9d8)) != 0;
        }

        // The real cause of "Player 2, edits made to the copied arsenal data..." (and then "no arsenals") in the lobby's
        // Arsenal Lab after a match, found with menu_debug (2026-11-14, developer PC): as it starts, the Laboratory reads
        // menu VARIABLE 0 (menu command 14 -> 1) and hands it to g5 #111 (0x140217e30), which stores it as the Lab's
        // player at [0x1404bec08] +0xb50; g5 #113 reads it back (1 = the second split-screen player: the warning, and
        // that player's empty arsenal list). The lobby keeps 0 or 1 in variable 0, and after a match it was 1
        // (OpenLobbyArsenalEditor only clears values above 1, which have no list behind them). While the Lab is the
        // lobby's, g5 #111 gets 0: always player 1.
        SessionCommandFn g_labPlayer = nullptr;
        int64_t HookedLabPlayer(int32_t* args)
        {
            if (args && args[0] != 0 && g_editorFromLobby)
            {
                Log(L"menu: the lobby's Arsenal Lab was told player %d - player 1 instead (not split-screen Player 2)", args[0] + 1);
                int32_t first[8] = {};
                memcpy(first + 1, args + 1, sizeof(int32_t) * 7);
                return g_labPlayer(first);
            }
            return g_labPlayer(args);
        }

        int64_t HookedLabPreviewMode(int32_t* args)
        {
            int64_t result = g_labPreviewMode(args);
            if (result == 1) return result;
            int menu = CurrentMenu(MenuManager());
            return menu == kEditorMenu || menu == kStockMenu ? 1 : result;
        }

        int64_t HookedPreview(int64_t* args)
        {
            int what = args ? static_cast<int>(*args) : 0;
            int gameState = *reinterpret_cast<int32_t*>(At(0x1404bc950));
            bool noPlayer = *reinterpret_cast<uint64_t*>(At(0x1404bc9d8)) == 0;
            if ((what == 102 || what == 104) && (g_previewTown ? !PreviewPlayerReady() : noPlayer))
            {
                if (!g_previewTown)
                {
                    if (g_previewTownClosing)
                    {
                        if (!g_previewTownAgain) Log(L"preview: asked for while the town session ends (game state %d) - again after", gameState);
                        g_previewTownAgain = true;
                    }
                    else if (MenuGameState(gameState)) StartPreviewTown(gameState);
                    else
                    {
                        Log(L"preview: no player and game state %d - not started", gameState);
                        return 0;
                    }
                }
                if (what == 102) g_previewHeld = 102;
                else g_previewHeldSkill = true;
                return 0; // held: PreviewTick starts it when the town runs
            }
            if (what == 105 && (g_previewTown || g_previewTownClosing) && !PreviewPlayerReady())
            {
                g_previewHeld = 0; // not running yet: nothing to stop
                g_previewHeldSkill = false;
                g_previewTownAgain = false;
                return 1;
            }
            return g_preview(args);
        }

        uint32_t SafeU32(uint64_t address)
        {
            __try { return *reinterpret_cast<uint32_t*>(address); }
            __except (EXCEPTION_EXECUTE_HANDLER) { return 0xdeadbeef; }
        }

        // g5 #140 (FUN_1402183c0): its second argument 0 hides the town's scene layers, 1 shows them.
        void PreviewTownLayers(bool show)
        {
            int64_t args[3] = { show ? 1 : 0, show ? 1 : 0, show ? 1 : 0 };
            reinterpret_cast<int64_t (*)(int64_t*)>(At(0x1402183c0))(args);
        }

        void EndPreviewTown()
        {
            PreviewTownLayers(true); // as the story's Laboratory does on the way out
            // As g4 #458 (FUN_1402117e0) does for a town session (mode 0).
            *reinterpret_cast<int32_t*>(At(0x1404bc5d8)) = 6;
            *reinterpret_cast<int32_t*>(At(0x1404bbec8)) = 1;
            *reinterpret_cast<int32_t*>(At(0x1404bbecc)) = 1;
            *reinterpret_cast<int32_t*>(At(0x1404bc8bc)) = 1;
            *reinterpret_cast<int32_t*>(At(0x1404bc9b0)) = -1;
            reinterpret_cast<void (*)()>(At(0x14012b4f0))();
        }

        // Diagnostics for the hang without story progress (2026-11-12, read live on the developer PC with a fresh save):
        // the preview stuck in state 4 because the town's frame stopped. FUN_14012b110 (state table at 0x1402dd1a0) is
        // only called from FUN_140129ac0, called from FUN_14017cd00: the update (vtable slot 3, 0x14047bc30) of the scene
        // object at [0x1404c51e8], whose own gate 0x140509a44 was 1. So the game's task runner stopped calling it. Its
        // update calls are counted, its header (+8..+0x48) logged on each change while pdnet's session is there, and a
        // stall of the count logged once with the game's states.
        using SceneUpdateFn = void (*)(void*);
        SceneUpdateFn g_sceneUpdate = nullptr;
        std::atomic<uint32_t> g_sceneUpdates{ 0 };
        void HookedSceneUpdate(void* self)
        {
            ++g_sceneUpdates;
            g_sceneUpdate(self);
        }

        void WatchSceneUpdates()
        {
            static bool done = false;
            if (done) return;
            done = true;
            auto slot = reinterpret_cast<uint64_t*>(At(0x14047bc30));
            if (*slot != reinterpret_cast<uint64_t>(At(0x14017cd00))) { Log(L"preview: scene update slot not as expected - not counted"); return; }
            g_sceneUpdate = reinterpret_cast<SceneUpdateFn>(*slot);
            uint64_t hook = reinterpret_cast<uint64_t>(&HookedSceneUpdate);
            Write(reinterpret_cast<uint8_t*>(slot), std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&hook), reinterpret_cast<uint8_t*>(&hook) + 8));
        }

        void SceneDiagnostics()
        {
            WatchSceneUpdates();
            if (!g_sceneUpdate || !(g_previewTown || g_previewTownClosing)) return;
            static uint8_t last[0x40] = {};
            static uint32_t lastCount = 0;
            static int still = 0;
            static bool toldStall = false;
            uint8_t* scene = *reinterpret_cast<uint8_t**>(At(0x1404c51e8));
            uint8_t now[0x40] = {};
            if (scene) memcpy(now, scene + 8, sizeof(now));
            uint32_t count = g_sceneUpdates;
            auto hex = [](const uint8_t* bytes)
            {
                std::wstring text;
                for (int i = 0; i < 0x40; ++i) { wchar_t two[4]; swprintf_s(two, L"%02X", bytes[i]); text += two; if (i % 8 == 7) text += L' '; }
                return text;
            };
            if (memcmp(now, last, sizeof(now)) != 0)
            {
                Log(L"preview: scene +8: %s (updates %u, game state %d, preview state %d)", hex(now).c_str(), count,
                    *reinterpret_cast<int32_t*>(At(0x1404bc950)), *reinterpret_cast<int32_t*>(At(0x140534798)));
                memcpy(last, now, sizeof(now));
            }
            still = count == lastCount ? still + 1 : 0;
            if (count != lastCount) toldStall = false;
            lastCount = count;
            // The safety net (Experimental 154): while the game doesn't call it, the update runs from the menu frame (same
            // thread), so the preview's states and the session's end go on and leaving the Lab doesn't hang. Called on the
            // original, not the hook, so the count shows when the game itself calls it again.
            if (still >= 10 && scene && GetConfig().previewUnstick)
            {
                static int runs = 0;
                if (runs++ % 300 == 0) Log(L"preview: running the town's update from the menu frame (stalled; %d so far)", runs);
                g_sceneUpdate(scene);
            }
            if (still == 10 && !toldStall)
            {
                toldStall = true;
                Log(L"preview: the scene's update stopped (count %u) - game state %d, preview state %d, mode %d, bbc36 %d, 509a44 %d, bc41c %d, 51c128 %d, menu state %d",
                    count, *reinterpret_cast<int32_t*>(At(0x1404bc950)), *reinterpret_cast<int32_t*>(At(0x140534798)), *reinterpret_cast<int32_t*>(At(0x1404bc9d4)),
                    *At(0x1404bbc36), *reinterpret_cast<int32_t*>(At(0x140509a44)), *reinterpret_cast<int32_t*>(At(0x1404bc41c)),
                    *reinterpret_cast<int32_t*>(At(0x14051c128)), *reinterpret_cast<int32_t*>(At(0x140533744)));
            }
        }

        void UnpauseUnderPreview(); // (below)

        void PreviewTick()
        {
            UnpauseUnderPreview();
            SceneDiagnostics();
            int state = *reinterpret_cast<int32_t*>(At(0x140534798));
            int gameState = *reinterpret_cast<int32_t*>(At(0x1404bc950));
            int mode = *reinterpret_cast<int32_t*>(At(0x1404bc9d4));
            uint64_t player = *reinterpret_cast<uint64_t*>(At(0x1404bc9d8));
            uint32_t loaded = player ? SafeU32(player + 0x20) : 0, ready = player ? SafeU32(player + 0x28) : 0;
            static int lastState = -1, lastGame = -1, lastMode = -1;
            static uint64_t lastPlayer = 1;
            static uint32_t lastLoaded = 1, lastReady = 1;
            if (state != lastState || gameState != lastGame || mode != lastMode || player != lastPlayer || loaded != lastLoaded || ready != lastReady)
            {
                if (g_previewTown || state != 0 || lastState > 0)
                    Log(L"preview: state %d, game state %d, mode %d, player %llX (+20 %u, +28 %u)%s", state, gameState, mode, player, loaded, ready,
                        g_previewTown ? L" (pdnet's town session)" : L"");
                if (state == 3 && lastState != 3)
                {
                    // Diagnostics for the dark background: the screen fade (DAT_14052df20) and the stage, to compare the
                    // story's Lab with pdnet's town session.
                    uint64_t fade = *reinterpret_cast<uint64_t*>(At(0x14052df20));
                    std::wstring fields;
                    if (fade)
                        for (int offset : { 0x40, 0x48, 0x50, 0x54, 0x58, 0x60, 0x70, 0x74, 0x78, 0x7c, 0x80 })
                        {
                            wchar_t one[32];
                            swprintf_s(one, L" +%X=%X", offset, SafeU32(fade + offset));
                            fields += one;
                        }
                    Log(L"preview: running - stage %d, menu state %d, 530034 %X, fade %llX%s", *reinterpret_cast<int32_t*>(At(0x1403e0304)),
                        *reinterpret_cast<int32_t*>(At(0x140533744)), *reinterpret_cast<uint32_t*>(At(0x140530034)), fade, fields.c_str());
                }
                lastState = state; lastGame = gameState; lastMode = mode; lastPlayer = player; lastLoaded = loaded; lastReady = ready;
            }
            // Game state 2 (loading) moves on (FUN_14012a110, to state 3) only when the session has loaded (0x1404bbca4,
            // set by FUN_140129fb0) and the menu system's state 0x140533744 is 3 (no menu open): the story starts the
            // town before opening any menu. Here the Lab stays open, so pdnet takes that step itself once loaded.
            if (g_previewTown && gameState == 2)
            {
                // The town session loads menu\Town.alr into the first free texture slot (FUN_140197510) and keeps the
                // slot's number at [0x1404bec20] +0x48, which FUN_14015eaf0 then reads as a status: 1-2 loading, 0 done.
                // From the multiplayer menu the slot is 1 or 2; in the lobby its own textures hold the low slots, the
                // town got slot 3, and 3 counts as nothing: state 2 waited forever. 3 or more becomes 2 (loading), and
                // the game's loader (FUN_140197750) finishes it as usual.
                uint64_t ec20 = *reinterpret_cast<uint64_t*>(At(0x1404bec20));
                if (ec20 && SafeU32(ec20 + 0x40) == 6 && SafeU32(ec20 + 0x48) >= 3 && SafeU32(ec20 + 0x48) < 16)
                {
                    Log(L"preview: Town.alr went to texture slot %u - its load counted as loading", SafeU32(ec20 + 0x48));
                    *reinterpret_cast<uint32_t*>(ec20 + 0x48) = 2;
                }
                // Diagnostics (lobby: it stayed in state 2): what the main loop's state 2 and FUN_140129fb0 look at.
                static ULONGLONG lastTold = 0;
                ULONGLONG now = GetTickCount64();
                if (now - lastTold > 1000)
                {
                    lastTold = now;
                    uint64_t ec20 = *reinterpret_cast<uint64_t*>(At(0x1404bec20));
                    Log(L"preview: in state 2: loaded %d, menu state %d, paused %d, bbc36 %d, c01dc %d, c015c %d, fd90 %d, fee0 %d, ec20+40 %u +48 %u, bc8d8 %d",
                        *reinterpret_cast<int32_t*>(At(0x1404bbca4)), *reinterpret_cast<int32_t*>(At(0x140533744)), *reinterpret_cast<int32_t*>(At(0x1404bc41c)),
                        *reinterpret_cast<uint8_t*>(At(0x1404bbc36)), *reinterpret_cast<int32_t*>(At(0x1404c01dc)), *reinterpret_cast<int32_t*>(At(0x1404c015c)),
                        *reinterpret_cast<int32_t*>(At(0x14058fd90)), *reinterpret_cast<int32_t*>(At(0x14058fee0)),
                        ec20 ? SafeU32(ec20 + 0x40) : 0, ec20 ? SafeU32(ec20 + 0x48) : 0, *reinterpret_cast<int32_t*>(At(0x1404bc8d8)));
                }
            }
            if (g_previewTown && gameState == 2 && *reinterpret_cast<int32_t*>(At(0x1404bbca4)) != 0 && *reinterpret_cast<int32_t*>(At(0x140533744)) != 3)
            {
                Log(L"preview: town session loaded under the menu (menu state %d) - letting it start", *reinterpret_cast<int32_t*>(At(0x140533744)));
                reinterpret_cast<void (*)()>(At(0x14012a110))();
            }
            // The town's HUD (mini-map) flashed up as the session took over under the menu. While pdnet's session runs,
            // its HUD objects keep the hidden flag 0x40000 (what the game sets on them when a session ends,
            // FUN_14012b600 / FUN_14012b7a0), and the area-name banner bits (0x18000 at 0x140530034) stay off.
            if (g_previewTown && gameState >= 1 && gameState <= 6)
            {
                for (uint64_t hud : { 0x1404bbd50ull, 0x1404bcc50ull })
                    if (uint64_t object = *reinterpret_cast<uint64_t*>(At(hud))) *reinterpret_cast<uint32_t*>(object + 8) |= 0x40000;
                // (Clearing bit 1 at +0x40 of 0x1404bbd50's object as well, which FUN_14012a110 sets, crashed when the
                // player left the editor: left as it is.)
                *reinterpret_cast<uint32_t*>(At(0x140530034)) &= ~0x18000u;
                if (gameState == 6) KeepPreviewTrack(L"in the session");
            }
            // (Cutting the end's waits in game states 10 and 11, 0x1404bc8c0, to a frame crashed in the menu's drawing
            // right after: they stay as the game has them.)
            if (g_previewTownClosing && MenuGameState(gameState))
            {
                // A session started with "menu after" -1 ends in game state 13 (FUN_14012b7a0), not the menus' 0, and
                // the lobby's join then failed: the tester toaf, right after a preview, was "half-joined" (P2P link up,
                // then the join-error menus 84 / 53 after 6 s). The menus are back at 0 once the session has closed.
                if (gameState == 13)
                {
                    *reinterpret_cast<int32_t*>(At(0x1404bc950)) = 0;
                    Log(L"preview: town session closed - game state 13 -> 0 (the menus' own)");
                }
                g_previewTownClosing = false;
                if (!g_previewTownAgain)
                {
                    // The pause object (0x1404c0130) goes to the town's mode 6 as the session starts (FUN_14012ab20);
                    // a lobby player froze leaving the lobby after previews (2026-11-16, SNOW-MSI-PC, cause not seen).
                    // Back in the menus it returns to mode 0 (no pause handling), and the state is logged.
                    g_previewClosedAt = GetTickCount64();
                    if (int64_t pause = *reinterpret_cast<int64_t*>(At(0x1404c0130)))
                    {
                        int mode = *reinterpret_cast<int32_t*>(pause + 0x50);
                        Log(L"preview: town session closed - pause mode %d, flags 530034 %X", mode, *reinterpret_cast<uint32_t*>(At(0x140530034)));
                        if (mode != 0)
                        {
                            reinterpret_cast<void (*)(int64_t, int)>(At(0x140167580))(pause, 0);
                            Log(L"preview: pause mode %d -> 0 (the menus' own)", mode);
                        }
                    }
                    RestorePreviewZone();
                    // (RestoreMenuAudio here jammed the audio loader: 3 jobs left queued and the game stuck for good when
                    // the player backed out of the Lab at the same moment. Not called; the menus load their own sound.)
                    PreviewAreaBanner(true);
                    KeepPreviewTrack(L"after the session");
                    g_previewTrack = -1;
                }
                if (g_previewTownAgain)
                {
                    g_previewTownAgain = false;
                    StartPreviewTown(gameState);
                }
                else if (g_lobbyAvailableWhenClosed)
                {
                    g_lobbyAvailableWhenClosed = false;
                    SetLobbyAvailable(true);
                    Log(L"preview: town session closed - red hand off");
                }
            }
            if (!g_previewTown || !PreviewPlayerReady()) return;
            if (g_previewHeld)
            {
                int64_t start = g_previewHeld;
                g_previewHeld = 0;
                // The stage load hides the town's scene layers 9-13 and 29-31 (flag 0x20000; FUN_1401d1950) and g5 #140
                // with 1 shows them (also clears 0x140530034 flag 0x800, sets 0x1403dd3d5 and 0x1404bbee8). Read live in
                // the story's Lab (2026-11-07): all shown, both set. The preview room and the character are in them.
                PreviewTownLayers(true);
                Log(L"preview: the town runs - starting the held preview (town layers shown, as in the story's Lab)");
                g_preview(&start);
            }
            if (g_previewHeldSkill && state == 3)
            {
                int64_t skill = 104;
                g_previewHeldSkill = false;
                g_preview(&skill);
            }
            // The session ends as soon as the player is out of the two screens with the preview (the town's ambience,
            // a hum, played as long as it ran, and stayed through the rest of the Lab). Going straight back in is
            // handled: a preview asked for while it ends starts a new session afterwards.
            int menu = CurrentMenu(MenuManager());
            if (state == 0 && !g_previewHeld && !g_previewHeldSkill && menu != kEditorMenu && menu != kStockMenu)
            {
                g_previewTown = false;
                g_previewTownClosing = true;
                Log(L"preview: left the preview screens (menu %d) - ending the town session", menu);
                EndPreviewTown();
                // (Loading the menu's audio profile here, while the session still ran, crashed: FUN_14021a6a0 frees the
                // profile in use. It is loaded once the session has closed.)
            }
        }

        // Testers without story progress (toaf, mitocato) had the game stop for good as they left the editor or stock list,
        // the last log line being the preview's state 4 (stopping). If the menu frames stop for 3 s while pdnet's town
        // session is there, the game's main thread is paused for a moment and its call stack logged (Ghidra addresses),
        // up to 3 times per stop.
        void PreviewWatchdog()
        {
            std::thread([]
            {
                int reports = 0;
                for (;;)
                {
                    Sleep(500);
                    ULONGLONG since = GetTickCount64() - g_menuFrameAt;
                    if (since < 3000 || !(g_previewTown || g_previewTownClosing)) { if (since < 3000) reports = 0; continue; }
                    if (reports >= 3) continue;
                    ++reports;
                    HANDLE thread = OpenThread(THREAD_SUSPEND_RESUME | THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, g_menuFrameThread);
                    if (!thread) continue;
                    uint64_t frames[24] = {};
                    int count = 0;
                    if (SuspendThread(thread) != static_cast<DWORD>(-1))
                    {
                        CONTEXT context{};
                        context.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;
                        if (GetThreadContext(thread, &context))
                        {
                            // Unwinding reads only the modules' own tables: nothing here allocates while the thread is paused.
                            for (; count < 24 && context.Rip; ++count)
                            {
                                frames[count] = context.Rip;
                                DWORD64 base = 0;
                                PRUNTIME_FUNCTION entry = RtlLookupFunctionEntry(context.Rip, &base, nullptr);
                                if (!entry)
                                {
                                    context.Rip = *reinterpret_cast<DWORD64*>(context.Rsp);
                                    context.Rsp += 8;
                                    continue;
                                }
                                void* handlerData = nullptr;
                                DWORD64 establisher = 0;
                                RtlVirtualUnwind(UNW_FLAG_NHANDLER, base, context.Rip, entry, &context, &handlerData, &establisher, nullptr);
                            }
                        }
                        ResumeThread(thread);
                    }
                    CloseHandle(thread);
                    uint64_t image = reinterpret_cast<uint64_t>(At(kGhidraBase));
                    std::wstring stack;
                    for (int i = 0; i < count; ++i)
                    {
                        wchar_t one[48];
                        HMODULE module = nullptr;
                        wchar_t name[MAX_PATH] = L"?";
                        if (frames[i] >= image && frames[i] < image + 0x1000000) swprintf_s(one, L" %llx", frames[i] - image + kGhidraBase);
                        else
                        {
                            if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(frames[i]), &module))
                                GetModuleFileNameW(module, name, MAX_PATH);
                            const wchar_t* file = wcsrchr(name, L'\\');
                            swprintf_s(one, L" %s+%llx", file ? file + 1 : name, frames[i] - reinterpret_cast<uint64_t>(module));
                        }
                        stack += one;
                    }
                    Log(L"preview: the game has stopped for %llu s (preview state %d, game state %d, menu %d) - main thread at:%s", since / 1000,
                        *reinterpret_cast<int32_t*>(At(0x140534798)), *reinterpret_cast<int32_t*>(At(0x1404bc950)), CurrentMenu(MenuManager()), stack.c_str());
                }
            }).detach();
        }

        // Select under the preview (found by the tester Nuion, 2026-11-12: "if u press select in any arsenal editing it kills
        // the instance behind it"). While a town session runs, the pause object [0x1404c0130] (CPauseMenuClass, modes from
        // the table at 0x1403e7b70: handler, -, flags, next mode; switched by FUN_140167580) is in mode 6, whose handler
        // FUN_140166a50 switches to mode 14 when the pause button is pressed (FUN_1401669f0(1)). Mode 14 has flags 3: the
        // game's paused bit (0x140530034 bit 0) and the town's pause menu (menu 49 on a menu manager of its own), out of
        // sight under the Lab. The town's update then stopped: the preview froze, and leaving the editor hung on it.
        // While pdnet's session is there, the button handlers (FUN_140166a50, FUN_140166b30) do nothing.
        using PauseModeFn = void (*)(int64_t);
        PauseModeFn g_pauseButtons = nullptr, g_pauseButtons2 = nullptr;
        void HookedPauseButtons(int64_t self) { if (!g_previewTown && !g_previewTownClosing) g_pauseButtons(self); }
        void HookedPauseButtons2(int64_t self) { if (!g_previewTown && !g_previewTownClosing) g_pauseButtons2(self); }

        void NoPauseUnderPreview()
        {
            g_pauseButtons = reinterpret_cast<PauseModeFn>(At(0x140166a50));
            g_pauseButtons2 = reinterpret_cast<PauseModeFn>(At(0x140166b30));
            int patched = 0;
            for (int mode = 0; mode < 17; ++mode)
            {
                auto entry = reinterpret_cast<uint64_t*>(At(0x1403e7b70) + mode * 0x18);
                uint64_t hook = *entry == reinterpret_cast<uint64_t>(g_pauseButtons) ? reinterpret_cast<uint64_t>(&HookedPauseButtons)
                              : *entry == reinterpret_cast<uint64_t>(g_pauseButtons2) ? reinterpret_cast<uint64_t>(&HookedPauseButtons2) : 0;
                if (!hook) continue;
                if (Write(reinterpret_cast<uint8_t*>(entry), std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&hook), reinterpret_cast<uint8_t*>(&hook) + 8))) ++patched;
            }
            Log(L"gamepatch: skill preview: the town's pause button is off under the preview (%d pause modes)", patched);
        }

        // If the game is paused all the same while pdnet's session is there (bit 0 of 0x140530034), the pause object goes
        // back to the town's mode 6 (flags 0: the paused bit is cleared), so the town runs and leaving doesn't hang.
        void UnpauseUnderPreview()
        {
            if (!(g_previewTown || g_previewTownClosing) || !(*reinterpret_cast<uint32_t*>(At(0x140530034)) & 1)) return;
            int64_t pause = *reinterpret_cast<int64_t*>(At(0x1404c0130));
            if (!pause) return;
            int mode = *reinterpret_cast<int32_t*>(pause + 0x50);
            Log(L"preview: the game was paused under the preview (pause mode %d) - back to the town's mode 6", mode);
            reinterpret_cast<void (*)(int64_t, int)>(At(0x140167580))(pause, 6);
        }

        void SkillPreviewScene()
        {
            NoPauseUnderPreview();
            WrapCommand<MenuCommandFn>(0x1403eede0 + 134 * 8, 0x140208300, &HookedPreview, g_preview, L"skill preview");
            WrapCommand<SessionCommandFn>(0x1403f1d50 + 119 * 8, 0x140217f80, &HookedLabPreviewMode, g_labPreviewMode, L"skill preview in multiplayer");
            WrapCommand<SessionCommandFn>(0x1403f1d50 + 111 * 8, 0x140217e30, &HookedLabPlayer, g_labPlayer, L"lobby Lab player number");        }
    }

    void InstallEarlyHooks() { InstallBackButton(); }

    bool HookFileOpens()
    {
        static std::atomic<bool> done{ false };
        static std::mutex lock;
        if (done) return true;
        std::lock_guard guard(lock);
        if (!done && PatchCreateFile2()) done = true;
        return done;
    }

    bool HookCall(uint64_t siteAddress, uint64_t target, void* hook)
    {
        uint8_t* site = At(siteAddress);
        // A call, or a jump that ends a function in a call's place (e9): the hook returns to the same place either way.
        if ((site[0] != 0xe8 && site[0] != 0xe9) || site + 5 + *reinterpret_cast<int32_t*>(site + 1) != At(target)) return false;
        uint8_t* cave = CodeCave(16);
        if (!cave) return false;
        std::vector<uint8_t> toHook = { 0xff, 0x25, 0x00, 0x00, 0x00, 0x00 }; // jmp [rip+0]
        Put64(toHook, reinterpret_cast<uint64_t>(hook));
        std::vector<uint8_t> call = { site[0] };
        Put32(call, Rel32(site + 5, cave));
        return Write(cave, toHook) && Write(site, call);
    }

    namespace
    {
        // Peers dropped at a match's end (2026-11-13, diagnostics; Fisher King / Motley, a 3-player Battle Royale on
        // Stable 12): the last two players went down together, and in the same second each game dropped the others
        // from its peer list (0x140595ea8, 0x298-byte entries: +0 active, +4 slot, +8 ping countdown, +0xc keepalive
        // countdown, +0x28 address) and called itself the winner. Fisher's game had dropped the host, so back in the
        // lobby it gave up on the session (menus 84 / 53). pdnet's links were healthy. Two game functions clear an
        // entry: FUN_140126bd0, the keepalive (each call counts +0xc down; at 0 it sends message 6 and drops that
        // peer) and FUN_140126aa0, which drops every peer (called from several end-of-session paths). Fisher's two
        // drops came 0.1 s apart, which points at the keepalive. Both are hooked to log which one drops a peer, the
        // entry's counters and bytes, the highest keepalive value seen (the refill value) and how often it runs.
        using VoidFn = void (*)();
        VoidFn g_dropAllPeers = nullptr, g_peerKeepalive = nullptr;
        std::atomic<uint32_t> g_keepaliveCalls{ 0 };
        int32_t g_keepaliveMax = 0;

        struct PeerEntryView { int32_t active, slot, ping, keepalive; };
        PeerEntryView PeerEntry(int i)
        {
            auto e = reinterpret_cast<int32_t*>(At(0x140595ea8) + i * 0x298);
            return { e[0], e[1], e[2], e[3] };
        }

        std::wstring PeerBytes(int i)
        {
            const uint8_t* e = At(0x140595ea8) + i * 0x298;
            std::wstring text;
            for (int k = 0; k < 0x60; ++k) { wchar_t two[4]; swprintf_s(two, L"%02X", e[k]); text += two; if (k % 16 == 15) text += L' '; }
            return text;
        }

        void HookedDropAllPeers()
        {
            int32_t count = *reinterpret_cast<int32_t*>(At(0x140594ea4)), active = 0;
            for (int i = 0; i < 3; ++i) active += PeerEntry(i).active ? 1 : 0;
            if (active)
                Log(L"peers: the game drops every peer (%d active, %d listed; called from %llx, game state %d)", active, count,
                    GhidraAddress(_ReturnAddress()), *reinterpret_cast<int32_t*>(At(0x1404bc950)));
            // Leaving a lobby soon after a skill preview (the freeze above): the pause and town state at that moment.
            if (ULONGLONG closed = PreviewClosedAt(); closed && GetTickCount64() - closed < 120000)
            {
                int64_t pause = *reinterpret_cast<int64_t*>(At(0x1404c0130));
                Log(L"peers: %llu s after a skill preview: pause mode %d, flags 530034 %X, mode %d, bc41c %d", (GetTickCount64() - closed) / 1000,
                    pause ? *reinterpret_cast<int32_t*>(pause + 0x50) : -1, *reinterpret_cast<uint32_t*>(At(0x140530034)),
                    *reinterpret_cast<int32_t*>(At(0x1404bc9d4)), *reinterpret_cast<int32_t*>(At(0x1404bc41c)));
            }
            g_dropAllPeers();
        }

        // Cross lag (2026-11-19, a 4-player Tag Team, SUFDHSP / toaf / DD6A8L0 / 2HIO1FG): the Epic-relayed link between
        // SUFDHSP and toaf went quiet for about 4 s (EOS: "interrupted", then restored). Each game's keepalive ran out
        // (150 calls, about 4.5 s) and dropped the other, so from then on each PC played the match without the other.
        // While pdnet's link to that peer is still open, the countdown is now held at 2 for up to kKeepHoldMs: the game
        // waits for the peer (FUN_14012bbb0 holds frames while a peer's data is old), the link's reliable packets
        // arrive when EOS restores it, and the match goes on together. A peer that has left has no link, and is dropped
        // as before. The peer's EOS id is in its entry (+0x5e, 16 bytes; the sda endpoint snapshot).
        constexpr ULONGLONG kKeepHoldMs = 20000;
        ULONGLONG g_holdSince[16]{};

        std::string PeerEosId(int i)
        {
            const uint8_t* e = At(0x140595ea8) + i * 0x298 + 0x5e;
            std::string id;
            bool any = false;
            for (int k = 0; k < 16; ++k) { char two[3]; sprintf_s(two, "%02x", e[k]); id += two; any = any || e[k]; }
            return any ? id : std::string();
        }

        void HoldQuietPeers(int count)
        {
            ULONGLONG now = GetTickCount64();
            for (int i = 0; i < count && i < 16; ++i)
            {
                auto e = reinterpret_cast<int32_t*>(At(0x140595ea8) + i * 0x298);
                if (!e[0] || e[3] > 2)
                {
                    if (g_holdSince[i] && e[0]) Log(L"peers: peer %d (slot %d) is sending again after %llu ms - kept in the match", i, e[1], now - g_holdSince[i]);
                    g_holdSince[i] = 0;
                    continue;
                }
                std::string id = PeerEosId(i);
                if (id.empty() || !p2p::HasLink(id)) continue; // gone: the game drops it
                if (!g_holdSince[i])
                {
                    g_holdSince[i] = now;
                    Log(L"peers: peer %d (slot %d, %S) has gone quiet but its link is open - the game waits for it (up to %llu s)", i, e[1], id.substr(0, 8).c_str(), kKeepHoldMs / 1000);
                }
                if (now - g_holdSince[i] < kKeepHoldMs) e[3] = 2;
                else if (e[3] == 2) Log(L"peers: peer %d (slot %d) quiet for %llu s - the game drops it", i, e[1], (now - g_holdSince[i]) / 1000);
            }
        }

        void HookedPeerKeepalive()
        {
            ++g_keepaliveCalls;
            int32_t count = *reinterpret_cast<int32_t*>(At(0x140594ea4));
            HoldQuietPeers(count);
            PeerEntryView before[16] = {};
            for (int i = 0; i < count && i < 16; ++i)
            {
                before[i] = PeerEntry(i);
                if (before[i].active && before[i].keepalive > g_keepaliveMax)
                {
                    g_keepaliveMax = before[i].keepalive;
                    Log(L"peers: highest keepalive countdown seen: %d (the keepalive ran %u times so far)", g_keepaliveMax, g_keepaliveCalls.load());
                }
            }
            g_peerKeepalive();
            for (int i = 0; i < count && i < 16; ++i)
                if (before[i].active && !PeerEntry(i).active)
                    Log(L"peers: the game's keepalive dropped peer %d (slot %d; countdown was %d, ping %d, highest seen %d, keepalive runs %u; game state %d). Entry: %s",
                        i, before[i].slot, before[i].keepalive, before[i].ping, g_keepaliveMax, g_keepaliveCalls.load(),
                        *reinterpret_cast<int32_t*>(At(0x1404bc950)), PeerBytes(i).c_str());
        }

        void WatchPeerDrops()
        {
            static const uint8_t kDropAll[10] = { 0x48, 0x89, 0x5c, 0x24, 0x08, 0x48, 0x89, 0x6c, 0x24, 0x10 };
            g_dropAllPeers = reinterpret_cast<VoidFn>(InlineHook(0x140126aa0, reinterpret_cast<void*>(&HookedDropAllPeers), L"peer drop (all)", kDropAll));
            const std::vector<uint8_t> keepalive = { 0x4c, 0x8b, 0xdc, 0x56, 0x57, 0x48, 0x81, 0xec, 0x88, 0x04, 0x00, 0x00 };
            g_peerKeepalive = reinterpret_cast<VoidFn>(InlineHookBytes(0x140126bd0, keepalive, keepalive, reinterpret_cast<void*>(&HookedPeerKeepalive), L"peer keepalive"));
            Log(L"gamepatch: peer drops logged (drop all %s, keepalive %s)", g_dropAllPeers ? L"ready" : L"NOT ready", g_peerKeepalive ? L"ready" : L"NOT ready");
        }
    }

    void Apply()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            WatchPeerDrops();
            labtests::Install(); // the dead camera and Theatre playback
            film::Start();       // Theatre films: every online match is recorded
            {
                bool player = HookCall(0x1402148da, 0x14014f830, reinterpret_cast<void*>(&HookedDeck));
                bool opponent = HookCall(0x14021491f, 0x14014f830, reinterpret_cast<void*>(&HookedDeck));
                Log(L"theatre: film decks %s", player && opponent ? L"hooked" : L"NOT hooked (skills in films may crash)");
            }
            DynamicCases();
            if (!HookFileOpens()) Log(L"gamepatch: title screen: CreateFile2 import not found");
            g_bodyTags = GetConfig().nametagsOnBodies ? 1 : 0;
            NametagsAlwaysOn();
            PartnerLock();
            TestArsenalChoices();
            SkillDataGuard();
            HowToPlay();
            SkillPreviewScene();
            if (NametagsOnBodies())
            {
                DeadNametagColor(GetConfig().deadNametagColor);
                NametagColours();
                TrackPlayDead();
            }
            if (GetConfig().nametagDebug) NametagDebug();
            if (GetConfig().lobbyArsenalEditor) LobbyArsenalEditor();
            if (GetConfig().voiceChat || GetConfig().iconGallery) LobbyTalkIndicator();
            CharacterSelectHooks(); // after LobbyTalkIndicator: shares its sprite and text hooks
            bool micLabel = GetConfig().voiceChat && !GetConfig().voiceClassic; // classic: the option keeps its "Voice Chat" name
            QuickJoinLabels();
            if (GetConfig().shopMaxOut) ShopMaxOut();
            if (GetConfig().ratedLobbies) RatedLobbies();
            if (GetConfig().ratedLobbies && !g_drawText) WrapCommand<MenuCommandFn>(0x1403eefe0 + 23 * 8, 0x140207840, &HookedDrawText, g_drawText, L"session info rated text");
            RenameAtLoad(); // "Xbox Live" -> "Online" before a script runs
            MenuTextPatches(GetConfig().ratedLobbies, GetConfig().scoreboard); // also "Xbox Live" -> "Online" in scripts already loaded
            // The in-game Settings menu (what was Xbox Live Options): left / right on pdnet's rows.
            WrapCommand<NativeFn>(0x1403ddde0 + 3 * 8, 0x1400bde50, &HookedSettingFlag, g_settingFlag, L"no Cross Play option");
            WrapCommand<MenuCommandFn>(0x1403eefe0 + 37 * 8, 0x140207d50, &HookedPlaceWindow, g_placeWindow, L"settings window position");
            WrapCommand<MenuCommandFn>(0x1403eefe0 + 88 * 8, 0x140207650, &HookedButtonBar, g_buttonBar, L"settings save button");
            WrapCommand<MenuCommandFn>(0x1403eefe0 + 42 * 8, 0x140207f00, &HookedDrawWindow, g_drawWindow, L"settings save pop-up");
            WrapCommand<NativeFn>(0x1403fb108, 0x1402168a0, &HookedButtons, g_buttons, L"settings save confirmation");
            InstallBackButton(); // also takes session command 154, which the Settings rows use
            if (g_optionChanged) Log(L"gamepatch: in-game Settings ready (multiplayer menu > Online Settings)");
            if (WrapCommand<MenuCommandFn>(0x1403eefe0 + 40 * 8, 0x140207e20, &HookedAddItem, g_addItem, L"menu item names"))
                Log(L"gamepatch: menu items renamed as they are added (Quick Join, Lobby Browser%s)", GetConfig().scoreboard ? L", Global scoreboards only" : L"");
            SoundOptions(); // Options > Sound: Aura Particle Beeps
            MapSlots();     // new map slots in the map picker (after SoundOptions: it puts in the row hook)
            SignalPastTitle(); // pdlaunch's start-up volume lasts until then
            bool arsenal = GetConfig().lobbyArsenalEditor; // the Arsenal Lab button for clients too
            if (GetConfig().lobbyGamertagOption || micLabel || arsenal) LobbyScriptPatches(GetConfig().lobbyGamertagOption, micLabel, arsenal);
            // menu_debug wraps every entry of the menu and session command tables, so it goes in last (10 s after this), on
            // top of pdnet's own hooks: installed first, it made every later hook find "not the code this pdnet expects".
            if (GetConfig().menuDebug) std::thread([] { Sleep(10000); MenuDebug(); }).detach();
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

    // Advanced list variants online (see PublishVariant / ApplyMemberVariant).
    std::string VariantAttribute()
    {
        std::lock_guard guard(g_shareLock);
        return g_myVariant;
    }

    void LobbyVariants(const std::string& lobbyId, const std::vector<LobbyVariant>& members)
    {
        std::string me = lobbies::LocalUserId();
        std::lock_guard guard(g_shareLock);
        if (g_variantLobby != lobbyId)
        {
            Log(L"variants: online: game lobby %S (%zu member(s)), this PC's slot %d, its PD_VARIANT '%S'", lobbyId.c_str(), members.size(),
                *reinterpret_cast<int32_t*>(At(0x140527bc4)), g_myVariant.c_str());
            g_variantLobby = lobbyId;
            if (!g_myVariant.empty()) // a pick made before the lobby was known
            {
                std::string text = g_myVariant;
                std::thread([lobbyId, text]
                {
                    bool ok = lobbies::Update(lobbyId, lobbies::Changes{}, lobbies::Changes{ { { "PD_VARIANT", text } }, {} });
                    Log(L"variants: share: PD_VARIANT '%S' to lobby %S (on joining) - %s", text.c_str(), lobbyId.c_str(), ok ? L"ok" : L"FAILED");
                }).detach();
            }
        }
        std::set<std::string> present;
        for (const LobbyVariant& m : members)
        {
            if (m.userId == me) continue;
            present.insert(m.userId);
            auto it = g_memberVariant.find(m.userId);
            if (it != g_memberVariant.end() && it->second == m.value) continue;
            if (it == g_memberVariant.end() && m.value.empty()) { g_memberVariant[m.userId] = ""; continue; }
            g_memberVariant[m.userId] = m.value;
            ApplyMemberVariant(m.userId, m.name, m.value);
        }
        for (auto it = g_memberVariant.begin(); it != g_memberVariant.end();)
        {
            if (present.count(it->first)) { ++it; continue; }
            if (!it->second.empty()) ApplyMemberVariant(it->first, "(left the lobby)", "");
            it = g_memberVariant.erase(it);
        }
    }

    void LobbyVariantsLeft(const std::string& lobbyId)
    {
        std::lock_guard guard(g_shareLock);
        if (lobbyId != g_variantLobby) return;
        int mySlot = *reinterpret_cast<int32_t*>(At(0x140527bc4));
        for (auto& [user, slot] : g_memberSlot)
            if (slot != mySlot) ClearVariant(slot);
        Log(L"variants: online: left lobby %S - the other players' variants are off (%zu)", lobbyId.c_str(), g_memberSlot.size());
        g_memberSlot.clear();
        g_memberVariant.clear();
        g_variantLobby.clear();
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

    bool TestBattle() { return AiBattle(); }

    bool FilmVariant(int slot, char* out, size_t size)
    {
        if (!out || size == 0) return false;
        out[0] = 0;
        if (slot < 0 || slot >= 4) return false;
        std::string text;
        {
            std::lock_guard guard(g_variantLock);
            if (g_variants[slot].on) text = VariantText(slot, g_variants[slot].pick);
        }
        if (text.empty()) return false;
        strncpy_s(out, size, text.c_str(), _TRUNCATE);
        return true;
    }

    // Mind Reading is status effect 0x25. The game's voice rule (FUN_1400c61f0, run for each received voice buffer) plays
    // a buffer when the chat manager (*(DAT_1404bbc48 + 0x18)) has +0xac set or the sender's channel equals +0xa8 (the
    // team). During a match FUN_140187a90 calls FUN_140187180 for each slot every frame, which sets this PC's +0xa8 to
    // its team and +0xac to "has status 0x25". Read here (only a byte; the game keeps it current).
    bool MindReading()
    {
        __try
        {
            uint8_t* chat = *reinterpret_cast<uint8_t**>(At(0x1404bbc48));
            uint8_t* manager = chat ? *reinterpret_cast<uint8_t**>(chat + 0x18) : nullptr;
            return manager && manager[0xac] != 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return false;
        }
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
