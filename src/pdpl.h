#pragma once
// PDPL (Phantom Dust Plugin Loader, Torphedo's pd_loader_core.dll) in the game: pdlaunch injects it after pdnet, while
// the game is still suspended (mod loader step 1, Conceptual branch; see docs\MOD_LOADER_CODE.md).

namespace pdnet::pdpl
{
    // pdlaunch runs this in the game (export PdBeforePdpl) right before it loads pd_loader_core.dll. PDPL opens a console
    // window with AllocConsole and then reads commands from it forever. pdnet gets in first: it makes a console itself and
    // hides it (so PDPL's AllocConsole fails and no window shows), makes that console PDPL's standard output and error
    // (read back into LocalState\pdnet\pdpl.log every 100 ms; a C runtime writes to a console at once, while a pipe is
    // buffered in 4 KB blocks), and points standard input at an empty pipe (PDPL's command loop waits there instead of
    // spinning). Without a console (Microsoft Store copies) its output is a pipe. PDPL's C runtime takes these handles
    // when the DLL loads.
    void BeforeLoad();

    // From pdnet's set-up (injected mode): logs whether PDPL is in the game, and after 1 and 5 minutes how much CPU time
    // its threads have used (a busy loop would show as tens of seconds).
    void Report();

    // Safe mode: BeforeLoad writes LocalState\pdnet\mods-boot.txt and this deletes it once the title screen is passed. If
    // the game dies before that (a broken mod?), pdlaunch finds the file at the next start and leaves PDPL out once.
    void TitleReached();
}
