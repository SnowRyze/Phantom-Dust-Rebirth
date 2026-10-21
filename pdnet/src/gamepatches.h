#pragma once
// Small in-memory changes to the game's own code (PDUWP.exe), applied once at startup. Each one checks that the bytes
// it replaces are exactly the expected ones first, so a different game version is left alone. No files are changed.
namespace pdnet::gamepatches
{
    void Apply();

    // Nametags on dead bodies can be switched at any time: in a lobby the game host's setting applies to everyone
    // (xsapi/mpsd.cpp reads it from the host's lobby member attributes), otherwise this PC's nametags_on_bodies.
    void SetNametagsOnBodies(bool on, const wchar_t* why);
    void UseOwnSettings();

    // The lobby's invite button (no Xbox Live invites any more) opens the Arsenal editor; leaving it returns to the
    // lobby. Returns false when not in the lobby menu (or the menu hooks aren't installed).
    bool OpenLobbyArsenalEditor();

    // The game's own "Voice Chat" setting (lobby Options, settings flag 15; pdnet labels it "Mic"): false = off.
    // With voice_chat=on it switches only this PC's microphone in the EOS voice room.
    bool GameMicOn();
}
