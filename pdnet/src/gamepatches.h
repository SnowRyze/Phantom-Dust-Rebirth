#pragma once
// Small in-memory changes to the game's own code (PDUWP.exe), applied once at startup. Each one checks that the bytes
// it replaces are exactly the expected ones first, so a different game version is left alone. No files are changed.
namespace pdnet::gamepatches
{
    void Apply();
}
