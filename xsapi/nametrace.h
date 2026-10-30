#pragma once
// Debug aid (pdnet.ini: name_trace=on): finds which game code reads other players' names, to locate the code that
// draws nametags over players (and hides them on dead bodies).
//
// While a shared session has other members, it looks for their gamertags (UTF-16 and UTF-8) in the game's writable
// memory, then watches up to four of those copies at a time with the CPU's hardware breakpoints (debug registers on
// every thread). Each read is traced back through the call stack, and every distinct chain of PDUWP.exe functions that
// read a name is logged as "nametrace: ...", with addresses as Ghidra shows them (image base 0x140000000).
// Watched copies rotate every few seconds and memory is rescanned every 30 s. Off by default; for test matches only.
namespace pdnet::nametrace
{
    // Starts the trace thread once (no-op unless name_trace=on).
    void Start();
}
