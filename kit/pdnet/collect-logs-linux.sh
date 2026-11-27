#!/usr/bin/env bash
# The Linux counterpart of Collect-Log.bat: copies the logs the developers need after a test into one folder and shows
# the last game run's lobby and connection steps. Send the files in that folder.
#
#   bash collect-logs-linux.sh       (setup-linux.sh --logs does the same)
#   App menu: right-click Phantom Dust Rebirth > Collect logs (normal install and Flatpak), or
#   flatpak run io.github.snowryze.PhantomDustRebirth logs
#   --gui                            open the folder and say where it is in a short message (what the menu entry does)
#
# The folder: PhantomDust-logs-PC-TIME on your Desktop (home folder if there is no Desktop). For the Flatpak, which
# can't write there, in its data folder (~/.var/app/io.github.snowryze.PhantomDustRebirth/data/wine-uwp/logs/collected),
# opened in your file manager. Files (the first two named as Collect-Log.bat names them):
#   pdnet-log-PC-TIME.txt      pdnet's own log (LocalState\pdnet.log of the game)
#   pdlaunch-log-PC-TIME.txt   the launcher's steps, as pdlaunch.log on Windows: pdnet updates, game start and exit
#   wine-log-PC-TIME.txt       wine-uwp's log of the last game run (crashes, missing Windows parts)
#   system-PC-TIME.txt         distro, kernel, CPU, GPU, wine-uwp and kit versions, pdnet settings (never eos.ini)
#   and, when there are some: pdlaunch.exe's own log and the mod loader's (pdlaunch-game-log, pdpl-log), the Flatpak's
#   setup/launcher logs and PD Helper's log.
set -uo pipefail

family=Microsoft.MSEsper_8wekyb3d8bbwe
gui=
for a in "$@"; do
    case $a in
        --gui) gui=1 ;;
        -h|--help) sed -n '2,/^set -/{/^set -/d;s/^# \{0,1\}//;p}' "$0"; exit 0 ;;
        *) echo "collect-logs-linux: unknown option $a (see --help)" >&2; exit 2 ;;
    esac
done

kit=${PDREBIRTH_KIT:-$(cd "$(dirname "$0")" && pwd)}
inside=
[ -f /.flatpak-info ] && inside=1
# the Flatpak's kit is kept in its data folder (data/rebirth-kit); a kit unzipped anywhere else goes with the normal one
if [ -n "${WINE_UWP_DATA:-}" ]; then data=$WINE_UWP_DATA
elif [ -z "$inside" ] && [ -d "$kit/../prefix" ]; then data=$(cd "$kit/.." && pwd)
else data=${XDG_DATA_HOME:-$HOME/.local/share}/wine-uwp
fi

# --- the menu entry: the folder in the file manager and a short message, no log text --------------------------------
zen() { if [ "${LC_ALL:-}" = C ] || [ "${LC_ALL:-}" = POSIX ]; then LC_ALL=C.UTF-8 zenity "$@"; else zenity "$@"; fi; }
show() {   # kind (info/error), text; without zenity or kdialog a notification, else nothing
    if command -v zenity >/dev/null; then zen --"$1" --title="Phantom Dust Rebirth" --width=460 --text="$2" >/dev/null 2>&1
    elif command -v kdialog >/dev/null; then kdialog --title "Phantom Dust Rebirth" --"$([ "$1" = info ] && echo msgbox || echo error)" "$2" >/dev/null 2>&1
    elif command -v notify-send >/dev/null; then notify-send "Phantom Dust Rebirth" "$2" 2>/dev/null
    fi
    true
}
open_folder() { command -v xdg-open >/dev/null && xdg-open "$1" >/dev/null 2>&1; }
say() { printf '%s\n' "$*"; }

# --- where things are ------------------------------------------------------------------------------------------------
game=""
for f in "$data"/stage/*/identity.env; do
    case $f in *.new/*|*.old/*) continue ;; esac
    if [ -f "$f" ] && grep -q 'Microsoft\.MSEsper' "$f"; then game=$(dirname "$f"); break; fi
done
users=$data/prefix/drive_c/users
pdlog=$users/${USER:-$(id -un)}/AppData/Local/Packages/$family/LocalState/pdnet.log
[ -f "$pdlog" ] || pdlog=$(ls -t "$users"/*/AppData/Local/Packages/$family/LocalState/pdnet.log 2>/dev/null | head -1)
state=$(dirname "${pdlog:-/nonexistent/x}")

pc=$(uname -n 2>/dev/null | sed 's/[^A-Za-z0-9._-]/_/g'); pc=${pc:-linux}
stamp=$pc-$(date +%Y%m%d-%H%M)
if [ -n "$inside" ]; then
    base=$data/logs/collected
else
    base=$(xdg-user-dir DESKTOP 2>/dev/null || true)
    { [ -n "$base" ] && [ "$base" != "$HOME" ] && [ -d "$base" ] && [ -w "$base" ]; } || base=$HOME
fi
out=$base/PhantomDust-logs-$stamp
[ -e "$out" ] && out=$out-$(date +%S)
if ! mkdir -p "$out"; then
    say "Could not create $out."
    [ -n "$gui" ] && show error "The logs could not be saved: $out could not be created."
    exit 1
fi

# Copies a log; a very large one (Wine's, with a lot of debug output) keeps its start and its end, under Discord's 10 MB
copy() {   # source, name in the folder
    [ -s "$1" ] || return 1
    local size; size=$(stat -c %s "$1")
    if [ "$size" -gt 8000000 ]; then
        { head -c 1000000 "$1"; printf '\n\n[... %s bytes left out by collect-logs-linux.sh ...]\n\n' $((size - 7000000)); tail -c 6000000 "$1"; } > "$out/$2"
    else
        cp -f "$1" "$out/$2"
    fi
}
copy "$pdlog" "pdnet-log-$stamp.txt"; have_pdnet=$?
copy "$kit/pdlaunch.log" "pdlaunch-log-$stamp.txt"; have_launch=$?
[ -n "$game" ] && copy "$data/logs/$(basename "$game").log" "wine-log-$stamp.txt"
[ -n "$game" ] && copy "$data/logs/$(basename "$game")-before-start.log" "update-log-$stamp.txt"
copy "$data/logs/flatpak-setup.log" "flatpak-setup-log-$stamp.txt"
copy "$data/logs/flatpak-launcher.log" "flatpak-launcher-log-$stamp.txt"
copy "$data/logs/pdhelper.log" "pdhelper-log-$stamp.txt"
# where wine-uwp starts the game through pdlaunch.exe (it runs from the game's folder): its own log (mod sync, PDPL)
# and PDPL's (the mod loader's) output, as on Windows
[ -n "$game" ] && copy "$game/pdlaunch.log" "pdlaunch-game-log-$stamp.txt"
copy "$(dirname "$pdlog")/pdnet/pdpl.log" "pdpl-log-$stamp.txt"

# --- system ----------------------------------------------------------------------------------------------------------
settings() {   # an ini without comments and blank lines; webhook addresses are secrets, so only whether one is set
    [ -f "$1" ] && sed 's/^\xEF\xBB\xBF//; s/\r$//; /^[[:space:]]*;/d; /^[[:space:]]*#/d; /^[[:space:]]*$/d
        s/^\([^=]*webhook[^=]*=\)..*/\1(set; not copied)/; s/^/    /' "$1"
}
osr=/etc/os-release; [ -f /run/host/os-release ] && osr=/run/host/os-release   # in a Flatpak, the host's
{
    echo "Collected:   $(date '+%F %T %z')"
    if [ -n "$inside" ]; then
        echo "Install:     Flatpak"
        sed -n 's/^\(name\|branch\|app-commit\|runtime\|flatpak-version\)=/    \1: /p' /.flatpak-info
        wup=/app/opt/wine-uwp/bin/wine-uwp
    else
        echo "Install:     normal (setup-linux.sh)"
        wup=""
        for c in "${WINE_UWP:-}" "$(command -v wine-uwp 2>/dev/null || true)" ~/.local/opt/wine-uwp/bin/wine-uwp ~/wine-uwp/bin/wine-uwp; do
            if [ -n "$c" ] && [ -x "$c" ]; then wup=$c; break; fi
        done
    fi
    if [ -n "$wup" ]; then
        wdir=$(dirname "$(readlink -f "$wup")")/..
        echo "wine-uwp:    $(cat "$wdir/share/wine-uwp/VERSION" 2>/dev/null || echo '?') ($(readlink -f "$wdir")), $("$wdir/bin/wine" --version 2>/dev/null || echo 'wine ?')"
    else
        echo "wine-uwp:    not found"
    fi
    echo "Kit:         $(head -1 "$kit/kit-version.txt" 2>/dev/null | tr -d '\r' || echo '?') ($kit)"
    [ -f "$game/pdnet.dll" ] && [ -f "$kit/pdnet.dll" ] && ! cmp -s "$kit/pdnet.dll" "$game/pdnet.dll" && echo "             the game's pdnet.dll is not the kit's"
    echo "Data:        $data"
    echo "Game:        ${game:-not installed}"
    echo "OS:          $(sed -n 's/^PRETTY_NAME=//p' "$osr" 2>/dev/null | tr -d '"')"
    echo "Kernel:      $(uname -srm)"
    echo "Machine:     $(cat /sys/devices/virtual/dmi/id/sys_vendor /sys/devices/virtual/dmi/id/product_name 2>/dev/null | tr '\n' ' ')"
    echo "CPU:         $(sed -n 's/^model name[[:space:]]*: //p' /proc/cpuinfo | head -1), $(nproc) threads"
    echo "Memory:      $(awk '/^MemTotal/ { printf "%.1f GB", $2 / 1048576 }' /proc/meminfo)"
    if command -v lspci >/dev/null; then
        lspci 2>/dev/null | grep -E 'VGA compatible|3D controller|Display controller' | sed 's/^/GPU:         /'
    else
        for d in /sys/class/drm/card[0-9]*/device; do
            [ -f "$d/vendor" ] && echo "GPU:         $(cat "$d/vendor"):$(cat "$d/device") driver $(basename "$(readlink -f "$d/driver")")"
        done | sort -u
    fi
    echo "Session:     ${XDG_SESSION_TYPE:-?} ${XDG_CURRENT_DESKTOP:-}${SteamDeck:+ (Steam Deck)}${SteamGameId:+ (started from Steam)}"
    [ -z "$inside" ] && echo "Game running: $(pgrep -x PDUWP.exe >/dev/null 2>&1 && echo yes || echo no)"
    echo
    echo "pdnet.ini in the game's folder:";      settings "$game/pdnet.ini"
    echo "settings.ini (in-game Online Settings):"; settings "$state/pdnet/settings.ini"
    echo "local.env (how wine-uwp starts the game):"; settings "$game/local.env"
} > "$out/system-$stamp.txt" 2>/dev/null

# --- summary, as Collect-Log.bat shows it (in a terminal; the menu entry only says where the files are) -------------
if [ -n "$gui" ]; then
    :
elif [ $have_pdnet -ne 0 ]; then
    say "No pdnet.log yet. Start Phantom Dust once after the setup."
    if [ $have_launch -eq 0 ]; then
        say ""; say "pdlaunch.log:"; tail -n 10 "$out/pdlaunch-log-$stamp.txt" | sed 's/^/  /'
    fi
else
    start=$(grep -n 'pdnet loaded from' "$out/pdnet-log-$stamp.txt" | tail -1 | cut -d: -f1)
    if [ -n "$start" ]; then
        say ""; say "Last game run, lobby and connection steps:"
        tail -n +"$start" "$out/pdnet-log-$stamp.txt" | tr -d '\r' |
            grep -iE 'pdnet loaded|gamepatch|eos: signed in|eos: .*fail|lobby .* created|joined lobby|could not|left lobby|changed remotely|search .*session|sda: (connect|inbound|.*TIMED|EOS)|lobby: |p2p: |p2p-debug: ' |
            tail -n 40 | sed 's/^/  /'
    fi
fi
say ""
say "Saved in $out"
say "Send the files in that folder to the Rebirth developers."
if [ -n "$gui" ]; then
    text="Your logs are saved in:
$out

Send the files in this folder to the Rebirth developers."
    if [ $have_pdnet -ne 0 ]; then text="$text

(There is no pdnet log yet: Phantom Dust hasn't been started with Rebirth.)"; fi
    open_folder "$out" || true
    show info "$text"
fi
exit 0
