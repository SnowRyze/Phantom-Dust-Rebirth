// Mod loader test plugin (Conceptual, step 8). PDPL loads plugins/*.dll from the mods it mounts and runs their DllMain.
// This one only prints a line: PDPL's standard output is pdnet's pipe, so the line shows in LocalState\pdnet\pdpl.log
// and pdnet.log ("pdpl: [test_plugin] ..."). Built by make-test-mods.ps1.
//
// With TEST_PLUGIN_CORE defined it also calls a function of pd_loader_core.dll (plugin_get_module_handle) through its
// import table, as many PDPL plugins do. PDPL 1.2.2's pd_loader_core.dll exports nothing, so that one is expected NOT to
// load (and pdlaunch warns about it in the Mods list).
#include <windows.h>
#include <stdio.h>

#ifdef TEST_PLUGIN_CORE
__declspec(dllimport) void* plugin_get_module_handle(const char* filename);
#endif

BOOL WINAPI DllMain(HINSTANCE self, DWORD reason, LPVOID reserved)
{
    (void)self;
    (void)reserved;
    if (reason == DLL_PROCESS_ATTACH)
    {
#ifdef TEST_PLUGIN_CORE
        printf("[test_plugin_core] loaded by PDPL in process %lu; plugin_get_module_handle(\"test_plugin_core.dll\") = %p\n",
               GetCurrentProcessId(), plugin_get_module_handle("test_plugin_core.dll"));
#else
        printf("[test_plugin] loaded by PDPL in process %lu - plugins work\n", GetCurrentProcessId());
#endif
        fflush(stdout);
    }
    return TRUE;
}
