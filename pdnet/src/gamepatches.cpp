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
        // `argb` instead when the player's health (status[r15] +0x0c, the same field the visibility check used) is 0.
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
            constexpr size_t kCaveSize = 64;
            uint8_t* cave = CodeCave(kCaveSize);
            if (!cave)
            {
                Log(L"gamepatch: dead nametag colour skipped - no free space for the colour routine");
                return;
            }

            uint64_t colour = (static_cast<uint64_t>(argb) << 32) | argb;
            std::vector<uint8_t> code;
            auto rip = [&](size_t instructionLength) { return cave + code.size() + instructionLength; };
            // lea rax, [0x1404bc608]            (status records, one pointer per player)
            code.insert(code.end(), { 0x48, 0x8d, 0x05 });
            Put32(code, Rel32(rip(4), At(0x1404bc608)));
            // mov rax, [rax + r15*8]
            code.insert(code.end(), { 0x4a, 0x8b, 0x04, 0xf8 });
            // test rax, rax ; jz white
            code.insert(code.end(), { 0x48, 0x85, 0xc0, 0x74, 0x12 });
            // cmp dword [rax+0xc], 0 ; jne white
            code.insert(code.end(), { 0x83, 0x78, 0x0c, 0x00, 0x75, 0x0c });
            // mov rax, colour ; jmp store
            code.insert(code.end(), { 0x48, 0xb8 });
            for (int i = 0; i < 8; ++i) code.push_back(static_cast<uint8_t>(colour >> (8 * i)));
            code.insert(code.end(), { 0xeb, 0x07 });
            // white: mov rax, -1
            code.insert(code.end(), { 0x48, 0xc7, 0xc0, 0xff, 0xff, 0xff, 0xff });
            // store: mov [0x14054c9b0], rax ; mov [0x14054c9b8], rax ; ret
            code.insert(code.end(), { 0x48, 0x89, 0x05 });
            Put32(code, Rel32(rip(4), At(0x14054c9b0)));
            code.insert(code.end(), { 0x48, 0x89, 0x05 });
            Put32(code, Rel32(rip(4), At(0x14054c9b8)));
            code.push_back(0xc3);

            std::vector<uint8_t> call = { 0xe8 };
            Put32(call, Rel32(site + 5, cave));
            call.resize(expected.size(), 0x90);

            if (!Write(cave, code) || !Write(site, call))
            {
                Log(L"gamepatch: dead nametag colour failed (VirtualProtect error %lu)", GetLastError());
                return;
            }
            Log(L"gamepatch: dead nametag colour applied (ARGB %08X)", argb);
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

    void Apply()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            g_bodyTags = GetConfig().nametagsOnBodies ? 1 : 0;
            if (NametagsOnBodies()) DeadNametagColor(GetConfig().deadNametagColor);
        });
    }

    void SetNametagsOnBodies(bool on, const wchar_t* why)
    {
        uint8_t want = on ? 1 : 0;
        if (g_bodyTags.exchange(want) != want) Log(L"gamepatch: nametags on dead bodies %s (%s)", on ? L"on" : L"off", why);
    }

    void UseOwnSettings() { SetNametagsOnBodies(GetConfig().nametagsOnBodies, L"this PC's pdnet.ini"); }
}
