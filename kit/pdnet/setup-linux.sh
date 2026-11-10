#!/usr/bin/env bash
# Sets up pdnet for Phantom Dust on Linux, with wine-uwp (https://github.com/LiveManQuestion/wine-uwp, branch
# phantom-dust). The Linux counterpart of Setup.bat: run it from the kit's pdnet folder.
#
#   ./setup-linux.sh                  install Phantom Dust if needed, then set up pdnet (asks for your player name)
#   ./setup-linux.sh --name NAME      same, without asking
#   ./setup-linux.sh --iso FILE       use this Windows 11 ISO if wine-uwp isn't set up yet
#   ./setup-linux.sh --update         update this kit to the newest pdnet build now (the game also does it at start)
#   ./setup-linux.sh --remove         stop using pdnet (your pdnet saves are kept)
#   ./setup-linux.sh --logs           copy the logs to your home folder, to send after a test
#
# wine-uwp is looked for in $WINE_UWP (the wine-uwp script), on PATH, in ~/.local/opt/wine-uwp and ~/wine-uwp; if
# none can load pdnet, it offers to download the prebuilt one (github.com/LiveManQuestion/wine-uwp releases).
# The game's files are not changed: wine-uwp loads pdnet.dll into the game when it starts (local.env in the game's
# folder, WINE_UWP_INJECT), as pdlaunch.exe does on Windows. pdnet and its files are copied next to the game.
# wine-uwp runs before-start.sh from the game's folder at each start: there it calls this script with --auto-update.
set -euo pipefail

kit=$(cd "$(dirname "$0")" && pwd)
store_id=9PCDNBHR11MR
family=Microsoft.MSEsper_8wekyb3d8bbwe
inject="export WINE_UWP_INJECT='pdnet.dll!PdInjectedInit'"

say()  { printf '%s\n' "$*"; }
fail() { printf '\nERROR: %s\n' "$*" >&2; exit 1; }
ask()  { local a=""; read -r -p "$1 " a || true; printf "%s" "$a"; }

action=setup name="" iso="" auto=""
while [ $# -gt 0 ]; do
    case $1 in
        --name) name=${2:?--name needs a value}; shift ;;
        --iso) iso=${2:?--iso needs a file}; shift ;;
        --remove) action=remove ;;
        --logs) action=logs ;;
        --update) action=update ;;
        --auto-update) action=update auto=1 ;;   # at game start: nobody is there to answer
        -h|--help) sed -n '2,15p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) fail "unknown option $1 (see --help)" ;;
    esac
    shift
done

# --- wine-uwp -------------------------------------------------------------------------------------------------------
# The first one found that can load pdnet (its wineuwp-launch knows WINE_UWP_INJECT); an older wine-uwp elsewhere
# is left alone. Every wine-uwp shares the same data folder (prefix, game), so nothing has to be reinstalled.
wup="" old=""
for c in "${WINE_UWP:-}" "$(command -v wine-uwp 2>/dev/null || true)" ~/.local/opt/wine-uwp/bin/wine-uwp ~/wine-uwp/bin/wine-uwp; do
    [ -n "$c" ] && [ -x "$c" ] || continue
    if grep -qa WINE_UWP_INJECT "$(dirname "$(readlink -f "$c")")/../lib/wine/x86_64-windows/wineuwp-launch.exe" 2>/dev/null; then
        wup=$c; break
    fi
    old=${old:-$c}
done
# None (or only an older one): offer the prebuilt runtime from the wine-uwp fork's releases, into ~/.local/opt.
release=https://github.com/LiveManQuestion/wine-uwp/releases/latest/download
asset=wine-uwp-phantom-dust-x86_64.tar.xz
dl_tmp=""
download_wine_uwp() {
    local dest=$HOME/.local/opt
    [ -z "$auto" ] || fail "no wine-uwp that can load pdnet was found."
    [ -f "$kit/pdnet.dll" ] && [ -f "$kit/eos.ini" ] ||
        fail "pdnet.dll or eos.ini is missing from $kit. Unzip the whole kit (eos.ini may come separately) and run this again."
    command -v curl >/dev/null || fail "curl is needed to download wine-uwp."
    if [ -z "${1:-}" ]; then
        if [ -n "$old" ]; then
            say "Your wine-uwp ($old) is too old to load pdnet. A newer one can be downloaded to $dest/wine-uwp"
            say "(about 50 MB; your game, prefix and sign-in are kept)."
        else
            say "wine-uwp (the Wine build that runs Phantom Dust) was not found. It can be downloaded to $dest/wine-uwp (about 50 MB)."
        fi
        case $(ask "Download it now? [Y/n]") in [nN]*) fail "set WINE_UWP=/path/to/bin/wine-uwp to use your own build." ;; esac
    fi
    dl_tmp=$(mktemp -d)
    trap 'rm -rf "$dl_tmp"' EXIT
    local tmp=$dl_tmp
    curl -fL --progress-bar -o "$tmp/$asset" "$release/$asset" || fail "download failed ($release/$asset)."
    curl -fsL -o "$tmp/$asset.sha256" "$release/$asset.sha256" || fail "download failed ($release/$asset.sha256)."
    (cd "$tmp" && sha256sum -c --quiet "$asset.sha256") || fail "the download is damaged (checksum mismatch); run this again."
    mkdir -p "$dest"
    if [ -e "$dest/wine-uwp" ]; then
        rm -rf "$dest/wine-uwp.old"
        mv "$dest/wine-uwp" "$dest/wine-uwp.old"
        say "(The previous $dest/wine-uwp is kept as $dest/wine-uwp.old.)"
    fi
    tar -xJf "$tmp/$asset" -C "$dest" || fail "could not unpack $asset."
    wup=$dest/wine-uwp/bin/wine-uwp
    if ! "$dest/wine-uwp/bin/wine" --version >/dev/null 2>&1; then
        fail "the downloaded wine-uwp doesn't start on this system. It needs glibc 2.38 or newer (Ubuntu 24.04, Debian 13, Fedora 39, Mint 22 or newer)."
    fi
    say "wine-uwp installed in $dest/wine-uwp."
}
[ -n "$wup" ] || [ $action = remove ] || [ $action = logs ] || download_wine_uwp
# The prebuilt wine-uwp from before updates at game start can load pdnet, so it was kept above: offer the newer one.
updates_at_start() { grep -q before-start.sh "$(readlink -f "$wup")"; }
if [ $action = setup ] && [ -n "$wup" ] && [ "$wup" -ef ~/.local/opt/wine-uwp/bin/wine-uwp ] &&
   ! updates_at_start && ! pgrep -x PDUWP.exe >/dev/null 2>&1; then
    say "Your wine-uwp can't update pdnet when the game starts. A newer one can be downloaded to ~/.local/opt/wine-uwp"
    say "(about 50 MB; your game, prefix and sign-in are kept)."
    case $(ask "Download it now? [Y/n]") in [nN]*) ;; *) download_wine_uwp confirmed ;; esac
fi
if [ -z "$wup" ]; then
    [ -n "$old" ] && fail "your wine-uwp ($old) is too old to load pdnet. Run this without --remove/--logs to update it, or set WINE_UWP=/path/to/bin/wine-uwp."
    fail "wine-uwp was not found. Run this without --remove/--logs to download it, or set WINE_UWP=/path/to/bin/wine-uwp."
fi
bindir=$(dirname "$(readlink -f "$wup")")
libdir="$bindir/../lib/wine/x86_64-windows"
data=${WINE_UWP_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/wine-uwp}

# The installed Phantom Dust: the stage folder whose identity.env names the package.
find_game() {
    local f
    for f in "$data"/stage/*/identity.env; do
        case $f in *.new/*|*.old/*) continue ;; esac
        [ -f "$f" ] && grep -q 'Microsoft\.MSEsper' "$f" && { dirname "$f"; return; }
    done
    true
}
game=$(find_game)
running() { pgrep -x PDUWP.exe >/dev/null 2>&1; }
pdlog="$data/prefix/drive_c/users/$USER/AppData/Local/Packages/$family/LocalState/pdnet.log"
previous_name=""
[ -n "$game" ] && [ -f "$game/pdnet.ini" ] &&
    previous_name=$(sed -n 's/^\xEF\xBB\xBF//; s/^display_name=//p' "$game/pdnet.ini" | tr -d '\r' | head -1)
keep_name=$previous_name   # what --update passes on, "Player" included
[ "$previous_name" = Player ] && previous_name=""

# --- --logs ---------------------------------------------------------------------------------------------------------
if [ $action = logs ]; then
    out="$HOME/PhantomDust-logs-$(date +%Y%m%d-%H%M%S)"
    mkdir -p "$out"
    [ -n "$game" ] && cp -f "$data/logs/$(basename "$game").log" "$out/game.log" 2>/dev/null || true
    cp -f "$pdlog" "$out/pdnet.log" 2>/dev/null || true
    [ -n "$(ls -A "$out")" ] || { rmdir "$out"; fail "no logs found (has the game run yet?)"; }
    say "Logs copied to $out - send the files in it."
    exit 0
fi

# --- --remove -------------------------------------------------------------------------------------------------------
if [ $action = remove ]; then
    [ -n "$game" ] || fail "Phantom Dust is not installed in $data."
    running && fail "close Phantom Dust first."
    rm -f "$game"/{pdnet.dll,pdnet.ini,eos.ini,EOSSDK-Win64-Shipping.dll,xaudio2_9redist.dll,kit-version.txt,before-start.sh}
    if [ -f "$game/local.env" ]; then
        sed -i '/WINE_UWP_INJECT/d; /^# Phantom Dust Rebirth/d' "$game/local.env"
        grep -q '[^[:space:]]' "$game/local.env" || rm -f "$game/local.env"
    fi
    say "pdnet removed. Phantom Dust starts with Xbox Live again. (pdnet's saves are kept, in its LocalState\\pdnet folder.)"
    exit 0
fi

# --- --update -------------------------------------------------------------------------------------------------------
# What pdlaunch.exe does at game start on Windows (launcher/pdlaunch.cpp, CheckForUpdate): the newest
# pdnet-update-<build>.zip on this kit's channel from the GitHub releases, checked against the "sha256 <zip> <hex>" line
# in its notes, replaces the kit's files except eos.ini and pdnet.ini (old ones kept in update/backup until it worked).
# Then the new files are copied into the game; its pdnet.ini stays, as on Windows. With --auto-update (before-start.sh,
# at game start) a failure only ends this script, and a game with another pdnet.dll than the kit's is left alone.
if [ $action = update ]; then
    running && fail "close Phantom Dust first."
    # set up before newer wine-uwp only ends the game with its window when asked to (see "have wine-uwp load pdnet")
    if [ -f "$game/local.env" ] && grep -q WINE_UWP_INJECT "$game/local.env" && ! grep -q WINE_UWP_EXIT_WITH_WINDOW "$game/local.env"; then
        printf '# wine-uwp: end the game when its window is gone, as Windows does (setup-linux.sh)\n%s\n' \
            "export WINE_UWP_EXIT_WITH_WINDOW=1" >> "$game/local.env"
    fi
    command -v curl >/dev/null || fail "curl is needed to update."
    command -v python3 >/dev/null || fail "python3 is needed to update."
    [ -f "$kit/kit-version.txt" ] || fail "kit-version.txt is missing from $kit, so this kit's build is unknown."
    kv() { sed -n "$1" "$kit/kit-version.txt" | tr -d '\r' | sed 's/^\xEF\xBB\xBF//' | head -1; }
    label=$(kv 1p); build=$(kv 's/^Build: //p'); want_md5=$(kv 's/^pdnet\.dll MD5 //p' | tr 'A-F' 'a-f')
    # Conceptual kits are never published: an update would replace the concept with a master build.
    case $label in *Conceptual*) say "Updates are off for this kit (Conceptual build)."; exit 0 ;; esac
    case $label in *Experimental*) own_channel=experimental ;; *Stable*) own_channel=stable ;; *) own_channel=off ;; esac
    channel=""
    # update_channel: the kit's pdnet.ini, the game's copy, then the file in-game Options > Updates writes (later wins)
    for f in "$kit/pdnet.ini" "$game/pdnet.ini" "$data/prefix/drive_c/users/$USER/AppData/Local/Packages/$family/LocalState/pdnet/settings.ini"; do
        c=$(sed -n 's/^\xEF\xBB\xBF//; s/^update_channel=//p' "$f" 2>/dev/null | tr -d '\r' | tail -1 || true)
        if [ -n "$c" ]; then channel=$c; fi
    done
    channel=$(printf '%s' "$channel" | tr 'A-Z' 'a-z')
    case $channel in
        stable|experimental|off) ;;
        "") channel=$own_channel ;;
        # a typo must not count as stable, which would take an Experimental kit back to a Stable
        *) say "update_channel=$channel isn't stable, experimental or off; using this kit's own ($own_channel)."; channel=$own_channel ;;
    esac
    [ "$channel" = off ] && { say "Updates are off for this kit (update_channel=off, or a development build)."; exit 0; }
    have_md5=$(md5sum "$kit/pdnet.dll" | cut -c1-8)
    [ -z "$want_md5" ] || [ "$have_md5" = "$want_md5" ] ||
        fail "this kit's pdnet.dll isn't the one its kit-version.txt names (a build put in by hand), so it isn't updated."
    if [ -n "$auto" ] && ! cmp -s "$kit/pdnet.dll" "$game/pdnet.dll"; then
        say "The game's pdnet.dll isn't this kit's (a build put in by hand?), so it isn't updated at game start."
        exit 0
    fi
    # The new files into the game. Not set up yet: the whole setup (same name as before).
    update_game() {
        [ -f "$game/pdnet.dll" ] || exec bash "$kit/setup-linux.sh" ${keep_name:+--name "$keep_name"}
        cp -f "$kit/pdnet.dll" "$kit/EOSSDK-Win64-Shipping.dll" "$kit/kit-version.txt" "$game/"
    }

    repo=$(sed -n 's/^\xEF\xBB\xBF//; s/^update_repo=//p' "$kit/pdnet.ini" 2>/dev/null | tr -d '\r' | tail -1 || true)
    repo=${repo:-SnowRyze/Phantom-Dust-Rebirth}
    say "This kit: ${label:-unknown} (build ${build:-?}), $channel channel. Checking github.com/$repo..."
    json=$(curl -fsSL --connect-timeout 4 --max-time 10 -H 'Accept: application/vnd.github+json' "https://api.github.com/repos/$repo/releases?per_page=30") ||
        fail "GitHub could not be reached."
    pick=$(printf '%s' "$json" | python3 -I -c '
import json, re, sys
channel, build, label = sys.argv[1], int(sys.argv[2] or 0), sys.argv[3]
best = None
for rel in json.load(sys.stdin):
    if rel.get("prerelease") and channel != "experimental":
        continue
    for a in rel.get("assets", []):
        m = re.fullmatch(r"pdnet-update-(\d+)\.zip", a.get("name", ""))
        if m and (best is None or int(m.group(1)) > best[0]):
            best = (int(m.group(1)), a["browser_download_url"], a["name"], rel.get("body") or "")
# on the stable channel a kit that is not a Stable goes back to the newest Stable, even with a lower build number
revert = channel != "experimental" and "Stable" not in label
if best is None:
    print("none"); sys.exit()
if (best[0] == build) if revert else (best[0] <= build):
    print("current", best[0]); sys.exit()
m = re.search(r"sha256 " + re.escape(best[2]) + r" ([0-9a-fA-F]{64})", best[3])
print("update", best[0], best[1], m.group(1).lower() if m else "-", "revert" if revert else "newer")
' "$channel" "${build:-0}" "$label") || fail "could not read GitHub's answer."
    set -- $pick
    case $1 in
        none) fail "no update is published on the $channel channel yet." ;;
        current)
            say "Up to date (build ${build:-?})."
            if [ -n "$game" ] && [ -f "$game/pdnet.dll" ] &&
               { ! cmp -s "$kit/pdnet.dll" "$game/pdnet.dll" || ! cmp -s "$kit/kit-version.txt" "$game/kit-version.txt"; }; then
                say "The game's pdnet files aren't this kit's; copying the kit's."
                update_game
            fi
            exit 0 ;;
    esac
    new_build=$2 url=$3 sum=$4
    [ "$5" = revert ] && say "Stable channel: going back to the newest Stable build ($new_build)."
    [ "$sum" != - ] || fail "build $new_build has no sha256 line in its release notes, so it isn't installed."
    say "Downloading build $new_build..."
    dl_tmp=$(mktemp -d)
    trap 'rm -rf "$dl_tmp"' EXIT
    # a stalled download gives up, so it can't hold the game back for long
    curl -fL ${auto:+-sS} --connect-timeout 10 --speed-limit 10000 --speed-time 30 -o "$dl_tmp/update.zip" "$url" ||
        fail "download failed ($url)."
    [ "$(sha256sum "$dl_tmp/update.zip" | cut -d' ' -f1)" = "$sum" ] || fail "the download is damaged (sha256 mismatch); nothing was changed."
    python3 -I -m zipfile -e "$dl_tmp/update.zip" "$dl_tmp/new" && [ -f "$dl_tmp/new/pdnet/pdnet.dll" ] ||
        fail "could not unpack build $new_build; nothing was changed."

    # Swap the files in by renaming (a running script keeps reading its old copy), keeping the old ones until done.
    backup="$kit/update/backup"
    rm -rf "$kit/update"; mkdir -p "$backup"
    done_files=() failed=""
    for f in "$dl_tmp/new/pdnet/"*; do
        n=$(basename "$f")
        [ -f "$f" ] || continue
        case $n in eos.ini|pdnet.ini) continue ;; esac
        if [ -e "$kit/$n" ]; then mv -f "$kit/$n" "$backup/$n" || { failed=$n; break; }; fi
        mv -f "$f" "$kit/$n" || { failed=$n; [ -e "$backup/$n" ] && mv -f "$backup/$n" "$kit/$n"; break; }
        done_files+=("$n")
    done
    if [ -n "$failed" ]; then
        for n in "${done_files[@]}"; do if [ -e "$backup/$n" ]; then mv -f "$backup/$n" "$kit/$n"; fi; done
        fail "replacing $failed failed; the old kit is back."
    fi
    [ -f "$dl_tmp/new/Setup.bat" ] && [ -f "$kit/../Setup.bat" ] && mv -f "$dl_tmp/new/Setup.bat" "$kit/../Setup.bat"
    rm -rf "$kit/update"
    say "Kit updated to build $new_build."
    if [ -z "$game" ]; then
        say "Phantom Dust isn't installed yet: run bash $kit/setup-linux.sh to install it and set up pdnet."
        exit 0
    fi
    update_game
    say "pdnet build $new_build is in the game."
    exit 0
fi

# --- check the kit --------------------------------------------------------------------------------------------------
for f in pdnet.dll EOSSDK-Win64-Shipping.dll pdnet.ini; do
    [ -f "$kit/$f" ] || fail "the kit is incomplete: $f is missing from $kit. Unzip the whole kit and run this again."
done
[ -f "$kit/eos.ini" ] || fail "eos.ini is missing. The person who sent you the kit sends it separately; put it in $kit."
[ "$(grep -cE '^(ProductId|SandboxId|DeploymentId|ClientId|ClientSecret)=.+' "$kit/eos.ini")" -ge 5 ] ||
    fail "eos.ini is incomplete (it needs ProductId, SandboxId, DeploymentId, ClientId and ClientSecret)."
[ -f "$libdir/xaudio2_9.dll" ] || fail "$libdir/xaudio2_9.dll is missing from wine-uwp."
running && fail "close Phantom Dust first."

# --- install Phantom Dust if needed ---------------------------------------------------------------------------------
if [ -z "$game" ]; then
    say "Phantom Dust is not installed yet. It's free in the Microsoft Store: your Microsoft account must own it"
    say "(\"Get\" it once at https://apps.microsoft.com/detail/$store_id if you never did). It needs about 10 GB."
    if [ ! -f "$data/prefix/.wine-uwp-ready" ]; then
        if [ ! -d "$data/windows" ]; then
            say ""
            say "wine-uwp first needs some files from a Windows 11 ISO (Microsoft's free evaluation ISO, about 7 GB)."
            if [ -z "$iso" ]; then
                iso=$(ask "Path to a Windows 11 ISO, or press Enter to download it to ~/Downloads:")
                iso=${iso/#\~/$HOME}
            fi
            if [ -z "$iso" ]; then
                iso="$HOME/Downloads/Windows11.iso"
                mkdir -p "$HOME/Downloads"
                dl=$(find "$bindir/../share" -name download_iso.py -path '*harvest*' 2>/dev/null | head -1)
                [ -n "$dl" ] || fail "download_iso.py not found in wine-uwp; download the ISO yourself and pass --iso FILE."
                [ -f "$iso" ] || python3 -I "$dl" "$iso"
            fi
            [ -f "$iso" ] || fail "no ISO at $iso"
            say "Extracting the Windows files (a few minutes)..."
            "$wup" harvest "$iso"
            say "Done. You can delete $iso now if you like."
        fi
        say "Creating the Wine prefix..."
        "$wup" new-prefix
    fi
    say ""
    say "Downloading Phantom Dust. Sign in with the browser link it shows. A licence error about the newest version"
    say "(1.3.25854.70) is expected: it then takes 1.3.25854.2, which is the version pdnet is made for."
    # a newer wine-uwp takes an older version only when asked to (--allow-older); an older one does it by itself
    older=""
    grep -qa -- --allow-older "$(dirname "$(readlink -f "$wup")")/../lib/wine/x86_64-windows/wineuwp-install.exe" 2>/dev/null &&
        older=--allow-older
    "$wup" install-store "$store_id" $older
    game=$(find_game)
    [ -n "$game" ] || fail "the install didn't finish (see the messages above)."
fi
say "Phantom Dust is installed in $game"

# --- an older swap-style setup: put the original Xbox Live DLL back -------------------------------------------------
if [ -f "$game/Microsoft.Xbox.Services.orig.dll" ]; then
    say "This copy had pdnet in place of Microsoft.Xbox.Services.dll; putting the original back."
    mv -f "$game/Microsoft.Xbox.Services.orig.dll" "$game/Microsoft.Xbox.Services.dll"
fi

# --- name -----------------------------------------------------------------------------------------------------------
previous=$previous_name
tries=0
while [ -z "$name" ] && [ $tries -lt 3 ]; do
    prompt="Your player name (what the other players will see, max 32 characters)"
    [ -n "$previous" ] && prompt="$prompt [$previous - press Enter to keep it]"
    name=$(ask "$prompt:")
    [ -z "$name" ] && name=$previous
    tries=$((tries + 1))
done
name=$(printf '%s' "${name:-Player}" | tr -d '\r\n=$' | sed 's/^ *//; s/ *$//' | cut -c1-32)

# --- copy pdnet in --------------------------------------------------------------------------------------------------
cp -f "$kit/pdnet.dll" "$kit/EOSSDK-Win64-Shipping.dll" "$game/"
# pdnet reads it next to itself: the build on the main menu and the Updates row in Options
if [ -f "$kit/kit-version.txt" ]; then cp -f "$kit/kit-version.txt" "$game/"; fi
install -m 600 "$kit/eos.ini" "$game/eos.ini"
# EOS needs xaudio2_9redist.dll next to it for voice; Wine's own XAudio2 works under that name.
cp -f "$libdir/xaudio2_9.dll" "$game/xaudio2_9redist.dll"
# the kit's settings with the chosen name (as Setup.bat does)
esc=$(printf '%s' "$name" | sed 's/[\\&|]/\\&/g')
sed "s|^display_name=.*|display_name=$esc|" "$kit/pdnet.ini" > "$game/pdnet.ini"

# --- have wine-uwp load pdnet into the game -------------------------------------------------------------------------
touch "$game/local.env"
sed -i '/WINE_UWP_INJECT/d; /^# Phantom Dust Rebirth/d; /WINE_UWP_EXIT_WITH_WINDOW/d; /^# wine-uwp: end the game/d' "$game/local.env"
printf '# Phantom Dust Rebirth: pdnet loaded the way pdlaunch.exe does on Windows (setup-linux.sh)\n%s\n' "$inject" >> "$game/local.env"
# End Game and closing the window end the game (newer wine-uwp only does that when asked; this stays after --remove)
printf '# wine-uwp: end the game when its window is gone, as Windows does (setup-linux.sh)\n%s\n' \
    "export WINE_UWP_EXIT_WITH_WINDOW=1" >> "$game/local.env"

# --- look for updates at each game start (wine-uwp runs this, as Windows runs pdlaunch.exe) --------------------------
{
    printf '# Phantom Dust Rebirth: update pdnet before the game starts (written by setup-linux.sh)\n'
    printf 'export WINE_UWP=%q\n' "$wup"
    printf 'exec bash %q --auto-update\n' "$kit/setup-linux.sh"
} > "$game/before-start.sh"

say ""
say "Set up as '$name'. Start Phantom Dust with:"
say "  $wup run $(basename "$game")"
say "The first title screen should say \"Rebirth\". Your Xbox profile doesn't show while pdnet is on: make a new one."
say "After a test: $0 --logs     To stop using pdnet: $0 --remove"
if updates_at_start; then
    say "Updates: pdnet updates itself when the game starts (in-game Options > Updates; log: $data/logs/$(basename "$game")-before-start.log)."
    say "Keep this kit folder where it is: the updates go through it."
else
    say "Updates: this wine-uwp is too old to update pdnet at game start, so run $0 --update now and then."
fi
