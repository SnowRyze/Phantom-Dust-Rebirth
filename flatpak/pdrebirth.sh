#!/usr/bin/env bash
# The Flatpak's command. It wraps the kit's setup-linux.sh, which does the real work.
#
#   pdrebirth                      play; the first time, set up first (windows when there is no terminal)
#   pdrebirth setup [--name NAME]  set up (again) in the terminal: Windows components, the game, pdnet
#   pdrebirth kit ARGS             the kit's setup-linux.sh with ARGS (--update, --remove, --help ...)
#   pdrebirth ARGS                 wine-uwp with ARGS (list, stop, ...)
#
# Everything is kept in $XDG_DATA_HOME/wine-uwp (~/.var/app/io.github.snowryze.PhantomDustRebirth/data/wine-uwp):
#   windows/ prefix/ stage/ logs/   wine-uwp's own (Windows components, Wine prefix, the game, logs)
#   rebirth-kit/                    a writable copy of the kit: setup-linux.sh updates pdnet through it
#   iso/Windows11.iso               the Windows ISO while setting up (put your own there to skip the download)
set -euo pipefail

# Steam exports WINEDLLOVERRIDES=dxgi=n and more to what it starts, and wine-uwp lets the environment win over its
# own launch defaults: the game would then start without Wine's dxgi/d3d11.
unset WINEDLLOVERRIDES WINEDLLPATH WINEPREFIX WINESERVER WINELOADER WINEDEBUG WINEARCH WINEESYNC WINEFSYNC
# gamescope's Vulkan layer can't hook Wine's X11 swapchain from inside a Flatpak; present through XWayland instead
export ENABLE_GAMESCOPE_WSI=0

export WINE_UWP=/app/opt/wine-uwp/bin/wine-uwp
export WINE_UWP_DATA=${WINE_UWP_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/wine-uwp}
export USER=${USER:-$(id -un)}
data=$WINE_UWP_DATA
kit=$data/rebirth-kit
app_kit=/app/share/pdrebirth/kit
iso=$data/iso/Windows11.iso
log=$data/logs/flatpak-setup.log
need_gb=30   # the ISO, its unpacked image and the game's download, at their largest
mkdir -p "$data/logs" "$data/tmp"
export TMPDIR=$data/tmp   # the sandbox's /tmp is in memory
# started from a menu or from Steam, nobody sees the launcher's own errors: keep them
if [ ! -t 2 ]; then exec 2>> "$data/logs/flatpak-launcher.log"; fi

find_game() {
    local f
    for f in "$data"/stage/*/identity.env; do
        case $f in *.new/*|*.old/*) continue ;; esac
        if [ -f "$f" ] && grep -q 'Microsoft\.MSEsper' "$f"; then dirname "$f"; return; fi
    done
    true
}
is_set_up() { local g; g=$(find_game); [ -n "$g" ] && [ -f "$g/pdnet.dll" ] && [ -f "$g/before-start.sh" ]; }
player_name() {
    local g; g=$(find_game)
    [ -n "$g" ] && [ -f "$g/pdnet.ini" ] || return 0
    sed -n 's/^\xEF\xBB\xBF//; s/^display_name=//p' "$g/pdnet.ini" | tr -d '\r' | head -1
}
kit_build() { sed -n 's/^Build: //p' "$1/kit-version.txt" 2>/dev/null | tr -d '\r' | head -1; }

# The kit has to be writable (its updater replaces its files), so it is copied out of /app. A newer Flatpak brings a
# newer kit; a kit that has updated itself past the Flatpak's is left alone, and so is its pdnet.ini.
# Returns 0 when it copied something.
refresh_kit() {
    local have new
    new=$(kit_build "$app_kit"); have=$(kit_build "$kit")
    if [ -f "$kit/pdnet.dll" ] && [ "${have:-0}" -ge "${new:-0}" ]; then return 1; fi
    mkdir -p "$kit"
    if [ -f "$kit/pdnet.ini" ]; then cp -f "$kit/pdnet.ini" "$kit/pdnet.ini.keep"; fi
    cp -a "$app_kit/." "$kit/"
    if [ -f "$kit/pdnet.ini.keep" ]; then mv -f "$kit/pdnet.ini.keep" "$kit/pdnet.ini"; fi
    return 0
}

# Steam starts its games with LC_ALL=C, in which zenity refuses any text that isn't ASCII (a player name, say).
# Not a function: the windows are started in the background and closed by their process id.
zenity=(env LC_ALL=C.UTF-8 zenity)

# "STAGE x" and "PROGRESS done total" lines (wine-uwp's tools) as plain text, a line per 5 %
plain_progress() {
    awk '
        /^PROGRESS / { if ($3 > 0) { p = int($2 * 100 / $3); p -= p % 5; if (p != last) { last = p; print "  " p " %"; fflush() } } next }
        /^(RATE|SOURCE|PID) / { next }
        /^STAGE / { sub(/^STAGE /, ""); last = -1; print $0 "..."; fflush(); next }
        { print; fflush() }'
}

# The same lines for zenity --progress: a number moves the bar, "# text" is the line above it. Never 100: that
# would turn the window's OK button on, in the middle of the setup.
bar_progress() {
    awk '
        /^PROGRESS / { if ($3 > 0) { p = int($2 * 100 / $3); if (p > 99) p = 99; if (p != last) { last = p; print p; fflush() } } next }
        /^(RATE|SOURCE|PID) / || /(err|fixme|warn):/ || /^[[:space:]]*$/ { next }
        /^STAGE / { sub(/^STAGE /, ""); last = 0; print "# " $0 "..."; print 0; fflush(); next }
        { gsub(/[&<>]/, ""); print "# " substr($0, 1, 110); fflush() }'
}

# The whole first-time setup, without questions: $1 is the player name.
setup_steps() {
    local free
    refresh_kit || true
    if [ ! -d "$data/windows" ] && [ ! -f "$data/prefix/.wine-uwp-ready" ]; then
        free=$(df -BG --output=avail "$data" | tail -1 | tr -dc 0-9)
        if [ "$free" -lt "$need_gb" ]; then
            echo "ERROR: only $free GB free in $data; the setup needs about $need_gb GB (about 12 GB stay in use)." >&2
            return 1
        fi
        mkdir -p "$data/iso"
        echo "Windows components: Microsoft's Windows 11 evaluation ISO (about 7 GB, deleted afterwards)."
        python3 -I /app/opt/wine-uwp/share/wine-uwp/packaging/harvest/download_iso.py "$iso"
    fi
    bash "$kit/setup-linux.sh" --name "$1" --iso "$iso" < /dev/null
    if [ -d "$data/windows" ]; then rm -rf "$data/iso"; fi
}

setup_terminal() {
    local name=""
    while [ $# -gt 0 ]; do
        case $1 in --name) name=${2:?--name needs a value}; shift ;; *) echo "unknown option $1" >&2; exit 2 ;; esac
        shift
    done
    if [ -z "$name" ]; then
        name=$(player_name)
        read -r -p "Your player name (what the other players will see)${name:+ [$name]}: " answer || true
        name=${answer:-${name:-Player}}
    fi
    setup_steps "$name" 2>&1 | plain_progress | tee -a "$log"
}

# Without a terminal (app menu, Steam): the same steps behind a progress window. Cancel stops the setup; the ISO
# download resumes the next time.
setup_windows() {
    local name job view
    if [ -n "$(find_game)" ]; then
        "${zenity[@]}" --question --title="Phantom Dust Rebirth" --width=420 --ok-label="Set up" --cancel-label="Not now" \
            --text="Phantom Dust is installed, but Rebirth (pdnet) is not set up for it yet. This only takes a moment." || exit 0
    else
        "${zenity[@]}" --question --title="Phantom Dust Rebirth" --width=520 --ok-label="Set up" --cancel-label="Not now" --text="\
Phantom Dust is not installed yet. Setting it up downloads:

 - Microsoft's Windows 11 evaluation ISO (about 7 GB, deleted afterwards), for the Windows components the game needs
 - Phantom Dust from the Microsoft Store (about 10 GB)

You will be asked to sign in with your Microsoft account. It has to own Phantom Dust, which is free: \"Get\" it once at
https://apps.microsoft.com/detail/9PCDNBHR11MR

About $need_gb GB of free space are needed. This takes a while." || exit 0
    fi
    name=$("${zenity[@]}" --entry --title="Phantom Dust Rebirth" --text="Your player name (what the other players will see):" \
        --entry-text="$(player_name)") || exit 0
    name=${name:-Player}

    : > "$log"
    setsid "$0" setup-steps "$name" >> "$log" 2>&1 &
    job=$!
    tail -n +1 -f --pid="$job" "$log" | bar_progress | "${zenity[@]}" --progress --width=640 \
        --title="Phantom Dust Rebirth: setting up" --text="Starting..." &
    view=$!
    # the sign-in link and code also get a window of their own, on top of the scrolling one; it closes by itself
    # when the sign-in is done
    ( while kill -0 "$job" 2>/dev/null; do
          code=$(sed -n 's/^ *2\. Enter the code  *\([A-Z0-9]*\).*/\1/p' "$log" | tail -1)
          if [ -n "$code" ]; then
              "${zenity[@]}" --info --title="Phantom Dust Rebirth: Microsoft sign-in" --width=520 --ok-label="Hide" --text="\
Sign in with the Microsoft account that owns Phantom Dust.

On a phone or computer, open

<span size='x-large'><b>microsoft.com/link</b></span>

and enter the code

<span size='xx-large'><b>$code</b></span>

Nothing to press here: once you have signed in, this window closes and the download starts.
The code is valid for 15 minutes." &
              dialog=$!
              trap 'kill "$dialog" 2>/dev/null' EXIT TERM
              while kill -0 "$job" 2>/dev/null && kill -0 "$dialog" 2>/dev/null && ! grep -q '^Signed in' "$log"; do sleep 1; done
              break
          fi
          sleep 1
      done ) &
    code_view=$!
    wait -n "$job" "$view" || true
    kill "$code_view" 2>/dev/null || true
    if kill -0 "$job" 2>/dev/null; then          # the window was closed
        kill -- "-$job" 2>/dev/null || true
        WINEPREFIX="$data/prefix" /app/opt/wine-uwp/bin/wineserver -k 2>/dev/null || true
        exit 0
    fi
    kill "$view" 2>/dev/null || true
    if ! wait "$job" || ! is_set_up; then
        "${zenity[@]}" --text-info --title="Phantom Dust Rebirth: the setup did not finish" --width=760 --height=480 \
            --filename=<(tail -n 40 "$log" | plain_progress) || true
        exit 1
    fi
    "${zenity[@]}" --info --title="Phantom Dust Rebirth" --width=420 \
        --text="Set up as '$name'. The game starts now; its first title screen should say \"Rebirth\"." || true
}

case ${1:-} in
"")
    if ! is_set_up; then
        if [ -t 0 ]; then setup_terminal; else setup_windows; fi
        is_set_up || exit 1
    elif refresh_kit; then
        # a newer Flatpak: its kit into the game, with the same name
        bash "$kit/setup-linux.sh" --name "$(player_name)" < /dev/null >> "$log" 2>&1 || true
    fi
    app=$(basename "$(find_game)")
    # A wine-uwp that runs the game folder's before-start.sh at each start updates pdnet through it. With one that
    # doesn't (a build without that hook), do the update here, as the kit's own launcher does outside a Flatpak.
    if ! grep -q before-start.sh "$(readlink -f "$WINE_UWP")" 2>/dev/null; then
        bash "$kit/setup-linux.sh" --auto-update < /dev/null > "$data/logs/$app-before-start.log" 2>&1 ||
            echo "pdnet update failed: see $data/logs/$app-before-start.log" >&2
    fi
    exec "$WINE_UWP" run "$app"
    ;;
setup)       shift; setup_terminal "$@" ;;
setup-steps) setup_steps "${2:-Player}" ;;   # internal: setup_windows runs it in its own session
kit)         shift; refresh_kit || true; exec bash "$kit/setup-linux.sh" "$@" ;;
*)           exec "$WINE_UWP" "$@" ;;
esac
