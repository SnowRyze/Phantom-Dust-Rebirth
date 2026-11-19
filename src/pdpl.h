#pragma once
// PDPL (Phantom Dust Plugin Loader, Torphedo's pd_loader_core.dll) in the game: pdlaunch injects it after pdnet, while
// the game is still suspended (mod loader step 1, Conceptual branch; see docs\MOD_LOADER_CODE.md).

namespace pdnet::pdpl
{
    // pdlaunch runs this in the game (export PdBeforePdpl) right before it loads pd_loader_core.dll. PDPL opens a console
    // window with AllocConsole and then reads commands from it forever. pdnet gets in first: it makes a console itself and
    // hides it (so PDPL's AllocConsole fails and no window shows), and points standard input at an empty pipe (PDPL's
    // command loop waits there instead of spinning) and standard output and error at a pipe whose text goes to
    // LocalState\pdnet\pdpl.log. PDPL's C runtime takes these handles when the DLL loads.
    void BeforeLoad();

    // From pdnet's set-up (injected mode): logs whether PDPL is in the game, and after 1 and 5 minutes how much CPU time
    // its threads have used (a busy loop would show as tens of seconds).
    void Report();
}
