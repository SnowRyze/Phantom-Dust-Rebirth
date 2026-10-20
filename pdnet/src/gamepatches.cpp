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
                Apply(Patch{ L"nametags on dead bodies", 0x1401511be,
                             { 0x83, 0x79, 0x0c, 0x00, 0x0f, 0x84, 0x84, 0x01, 0x00, 0x00 }, 4,
                             { 0x90, 0x90, 0x90, 0x90, 0x90, 0x90 } });
            }
        });
    }
}
