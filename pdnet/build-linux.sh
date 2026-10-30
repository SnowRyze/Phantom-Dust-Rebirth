#!/bin/bash
# Builds pdnet on Linux with clang-cl + lld-link, the same way build.cmd does with MSVC:
# build/Microsoft.Xbox.Services.dll (x64, static CRT). Optional argument: the output folder name.
#
# Needs:
#   XWIN       the output of "xwin splat" (MSVC CRT + Windows SDK headers and libs; default ~/xwin)
#   WINRT_PROJ C++/WinRT projection of the Windows .winmd files (default $XWIN/cppwinrt-proj; see below)
#   EOS_SDK    the EOS SDK's SDK folder (with Include/eos_sdk.h); without it pdnet builds with PDNET_NO_EOS
#   and, the first time (xsapi/gen missing):
#   PDNET_CPPWINRT   cppwinrt.exe (NuGet Microsoft.Windows.CppWinRT, the same version as WINRT_PROJ), run under $WINE
#   PDNET_WINRT_REF  a folder of Windows .winmd files (Windows.*.winmd)
#   XSAPI_WINMD      the game's Microsoft.Xbox.Services.winmd
set -e
ROOT=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-build}
XWIN=${XWIN:-$HOME/xwin}
CC=${CLANG_CL:-clang-cl}
LINK=${LLD_LINK:-lld-link}

[ -d "$XWIN/sdk/include/um" ] || { echo "No Windows SDK in $XWIN (run xwin splat first)"; exit 1; }
# The SDK's own C++/WinRT headers come out of xwin mostly lowercase, so the Windows projection is generated with the
# same cppwinrt.exe instead: cppwinrt -in <folder of Windows .winmd> -out $WINRT_PROJ
WINRT_PROJ=${WINRT_PROJ:-$XWIN/cppwinrt-proj}
[ -f "$WINRT_PROJ/winrt/base.h" ] || { echo "No C++/WinRT projection in $WINRT_PROJ"; exit 1; }

if [ ! -f "$ROOT/xsapi/gen/module.g.cpp" ]; then
    [ -n "$PDNET_CPPWINRT" ] && [ -n "$PDNET_WINRT_REF" ] && [ -n "$XSAPI_WINMD" ] || {
        echo "xsapi/gen is missing: set PDNET_CPPWINRT, PDNET_WINRT_REF and XSAPI_WINMD to generate it"; exit 1; }
    python3 -I "$ROOT/xsapi/generate.py" "$XSAPI_WINMD"
fi

EOSFLAGS=(-DPDNET_NO_EOS)
if [ -n "$EOS_SDK" ] && [ -f "$EOS_SDK/Include/eos_sdk.h" ]; then
    EOSFLAGS=("-imsvc$EOS_SDK/Include")
else
    echo "warning: no EOS SDK headers (EOS_SDK=$EOS_SDK): building without EOS"
fi

CFLAGS=(--target=x86_64-pc-windows-msvc -mcx16 /nologo /c /std:c++20 /EHsc /MT /O2 /Z7 /W3 /permissive- /bigobj /utf-8
        /DUNICODE /D_UNICODE -Wno-unknown-pragmas -Wno-unused-command-line-argument -Wno-microsoft-include
        "-imsvc$XWIN/crt/include" "-imsvc$XWIN/sdk/include/ucrt" "-imsvc$XWIN/sdk/include/um"
        "-imsvc$XWIN/sdk/include/shared" "-imsvc$XWIN/sdk/include/winrt" "-imsvc$WINRT_PROJ")
XINC=("/I$ROOT/xsapi/gen" "/I$ROOT/xsapi" "/I$ROOT/xsapi/stubs")
JOBS=${JOBS:-$(nproc)}

mkdir -p "$ROOT/$OUT/obj/xsapi"
cd "$ROOT/$OUT"

# compile OBJDIR FILE...: one object per source, JOBS at a time; uses the FLAGS array; skips up-to-date objects
FAILED=0
compile() {
    local objdir=$1 f o
    shift
    for f in "$@"; do
        o="$objdir/$(basename "$f" .cpp).obj"
        [ "$o" -nt "$f" ] && continue
        while [ "$(jobs -rp | wc -l)" -ge "$JOBS" ]; do wait -n || FAILED=1; done
        echo "  $(basename "$f")"
        "$CC" "${FLAGS[@]}" "/Fo$o" "$f" &
    done
    while [ "$(jobs -rp | wc -l)" -gt 0 ]; do wait -n || FAILED=1; done
    [ "$FAILED" = 0 ] || { echo "BUILD FAILED"; exit 1; }
}

echo "1. pdnet core"
FLAGS=("${CFLAGS[@]}" "${EOSFLAGS[@]}")
CORE=()
for f in dllmain hooks gamesave log config net sda testpeer eostest credstore eossdk eosclock eos lobbies p2p \
         gamepatches gamepads discord scores; do
    CORE+=("$ROOT/src/$f.cpp")
done
compile obj "${CORE[@]}"

echo "2. XSAPI stand-in"
# no precompiled header: clang-cl's /Yc,/Yu are fussy, and every file includes xsapi_pch.h itself
FLAGS=("${CFLAGS[@]}" "${XINC[@]}")
compile obj/xsapi "$ROOT/xsapi/xsapi_pch.cpp" "$ROOT/xsapi/xsapi.cpp" "$ROOT/xsapi/mpsd.cpp" "$ROOT/xsapi/nametrace.cpp" \
    "$ROOT/xsapi/gen/module.g.cpp" "$ROOT"/xsapi/stubs/*.cpp

echo "3. link"
$LINK /nologo /DLL /OUT:Microsoft.Xbox.Services.dll /DEF:"$ROOT/src/pdnet.def" /DEBUG /OPT:REF /OPT:ICF \
    /LIBPATH:"$XWIN/crt/lib/x86_64" /LIBPATH:"$XWIN/sdk/lib/um/x86_64" /LIBPATH:"$XWIN/sdk/lib/ucrt/x86_64" \
    obj/*.obj obj/xsapi/*.obj \
    kernel32.lib ole32.lib oleaut32.lib runtimeobject.lib ws2_32.lib bcrypt.lib
echo "Built $ROOT/$OUT/Microsoft.Xbox.Services.dll"
