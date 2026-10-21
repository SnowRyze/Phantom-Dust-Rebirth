#include "pch.h"
#include "gamepatches.h"
#include "config.h"
#include "log.h"

namespace pdnet::gamepatches
{
    namespace
    {
        constexpr uint64_t kGhidraBase = 0x140000000;

        struct Patch
        {
            const wchar_t* name;
            uint64_t address;               // Ghidra address in PDUWP.exe
            std::vector<uint8_t> expected;  // bytes that must be there now (including some context before the change)
            size_t offset;                  // where the change starts within `expected`
            std::vector<uint8_t> replacement;
        };

        bool Apply(const Patch& p)
        {
            auto base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr));
            uint8_t* at = base + (p.address - kGhidraBase);
            if (memcmp(at, p.expected.data(), p.expected.size()) != 0)
            {
                Log(L"gamepatch: %s skipped - the game code is not what this pdnet expects (different game version?)", p.name);
                return false;
            }
            uint8_t* target = at + p.offset;
            DWORD old = 0;
            if (!VirtualProtect(target, p.replacement.size(), PAGE_EXECUTE_READWRITE, &old))
            {
                Log(L"gamepatch: %s failed (VirtualProtect error %lu)", p.name, GetLastError());
                return false;
            }
            memcpy(target, p.replacement.data(), p.replacement.size());
            VirtualProtect(target, p.replacement.size(), old, &old);
            FlushInstructionCache(GetCurrentProcess(), target, p.replacement.size());
            Log(L"gamepatch: %s applied", p.name);
            return true;
        }

        uint8_t* Image() { return reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr)); }
        uint8_t* At(uint64_t ghidra) { return Image() + (ghidra - kGhidraBase); }

        // Free space for a few bytes of extra code: the zero-filled tail of the last page of PDUWP.exe's .text
        // section (mapped executable, never used by the game).
        uint8_t* CodeCave(size_t size)
        {
            auto dos = reinterpret_cast<IMAGE_DOS_HEADER*>(Image());
            auto nt = reinterpret_cast<IMAGE_NT_HEADERS*>(Image() + dos->e_lfanew);
            auto section = IMAGE_FIRST_SECTION(nt);
            for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section)
            {
                if (memcmp(section->Name, ".text", 5) != 0) continue;
                uint64_t start = (section->VirtualAddress + section->Misc.VirtualSize + 15) & ~15ull;
                uint64_t pageEnd = (section->VirtualAddress + section->Misc.VirtualSize + 0xfff) & ~0xfffull;
                if (pageEnd - start < size + 16) return nullptr;
                uint8_t* cave = Image() + start + 16; // keep a little distance from the last real code
                for (size_t k = 0; k < size; ++k)
                    if (cave[k] != 0) return nullptr;
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
    }

    void Apply()
    {
        static std::once_flag once;
        std::call_once(once, []
        {
            if (GetConfig().nametagsOnBodies)
            {
                // FUN_140150c30 draws the white nametag over each other player only while their health
                // (status record +0x0c) is not zero:   cmp dword ptr [rcx+0xc],0 / jz skip_nametag
                // The jz becomes six NOPs, so the name stays on a player at 0 health (a dead body).
                // The checks after it are kept: status effect 8 (e.g. invisibility), on screen, +0x54/+0x5c.
                if (Apply(Patch{ L"nametags on dead bodies", 0x1401511be,
                                 { 0x83, 0x79, 0x0c, 0x00, 0x0f, 0x84, 0x84, 0x01, 0x00, 0x00 }, 4,
                                 { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 } }))
                {
                    DeadNametagColor(GetConfig().deadNametagColor);
                }
            }
        });
    }
}
