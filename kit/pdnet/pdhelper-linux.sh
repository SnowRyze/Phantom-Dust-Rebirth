#!/usr/bin/env bash
# Runs PD Helper (https://github.com/eradication0/PDHelper: arsenal editor, ModLoader) with Phantom Dust on wine-uwp.
# PD Helper is not part of the kit. Get it whenever you like: this script can download it, or you unzip it yourself
# anywhere and give its folder (the one with "PD Helper 4.1.exe") once.
#
#   bash pdhelper-linux.sh                 PD Helper from the remembered folder; the first time, offers to download it
#   bash pdhelper-linux.sh --download      download PD Helper 4.1 into ~/PDHelper (or into FOLDER if given), then start it
#   bash pdhelper-linux.sh FOLDER          PD Helper from FOLDER; the folder is remembered for later runs
#   bash pdhelper-linux.sh modloader       the mod loader's Skill Editor instead
#   bash pdhelper-linux.sh --prepare       only download/copy what's needed, start nothing
#   --flatpak / --host                     which install to use when both are set up (otherwise the normal one)
#   flatpak run io.github.snowryze.PhantomDustRebirth pdhelper [modloader]
#                                          Flatpak only, no script needed: downloads PD Helper onto the game's C: drive
#                                          the first time (C:\PDHelper below), then starts it
#
# Start the game and open a profile first, then click Load Profile in PD Helper.
# Works with both kinds of install:
#  - normal (setup-linux.sh): wine-uwp is found as setup-linux.sh finds it ($WINE_UWP, PATH, ~/.local/opt/wine-uwp,
#    ~/wine-uwp). .NET 6 goes into wine-uwp's data folder (tools/), not into the prefix; PD Helper runs from its folder.
#  - the Rebirth Flatpak (io.github.snowryze.PhantomDustRebirth): Wine in the Flatpak can't see your home folder, so
#    PD Helper is copied to the game's C: drive (C:\PDHelper) and .NET 6 to C:\dotnet. Each run copies over files that
#    are newer in your folder; what PD Helper saves there (profiles, mods) stays in
#    ~/.var/app/io.github.snowryze.PhantomDustRebirth/data/wine-uwp/prefix/drive_c/PDHelper.
# PD Helper has to run on the game's Wine and prefix (plain "wine" can't see the game). Needs curl and python3; the
# first run downloads .NET 6 from Microsoft (about 70 MB, checksummed).
set -euo pipefail

app=io.github.snowryze.PhantomDustRebirth
app_bin=/app/opt/wine-uwp/bin   # wine-uwp's path inside the Flatpak sandbox, not on the host
remembered=${XDG_CONFIG_HOME:-$HOME/.config}/phantom-dust-rebirth/pdhelper-folder
pdh_url=https://github.com/eradication0/PDHelper/releases/download/ReleaseV4.1/PDHELPERv4.1.zip
pdh_sha256=ed16e0a849564554d692f2d5ad4fbf7f475133b50aed9c99fc96c743ae0a9b04
pdh_default=$HOME/PDHelper

dotnet_version=6.0.36
dotnet_files=(
  "Runtime/$dotnet_version/dotnet-runtime-$dotnet_version-win-x64.zip 935db5c6cee19f2c016e67168bfae7b491044735de76c673abb3b125dd325fd5e779d7efe12ba80178d46689ae70a25e558a3fa846417d44c5f4ca256e7f4bf2"
  "WindowsDesktop/$dotnet_version/windowsdesktop-runtime-$dotnet_version-win-x64.zip cee88fef07643dceb3d34ea71b64eeae85b8e39e7042bc4d35bc3a1f1df29373681dc48e31c01c9f593ec412699f6762b6fc5817433283c89df35a27ce8885bf"
)
labels_url=https://raw.githubusercontent.com/Torphedo/SkillEditor/4d29771d38f49a668753977dea88c1f07f12c465/res/labels.yaml
labels_sha256=ff6d233d636a83e880d8ddd73bd59ab9728e3818a3cc9a33b415a604afd192bd

fail() { echo "pdhelper-linux: $*" >&2; exit 1; }
need() { command -v "$1" >/dev/null || fail "$1 is needed for the first run; install it and try again."; }
helper_in() { (cd "$1" 2>/dev/null && ls PD\ Helper*.exe 2>/dev/null | head -1) || true; }

action= want= folder= download=
for a in "$@"; do
  case $a in
    -h|--help) sed -n '2,/^set -e/{/^set -e/d;s/^# \{0,1\}//;p}' "$0"; exit 0 ;;
    --flatpak) want=flatpak ;;
    --host)    want=host ;;
    --download) download=1 ;;
    --prepare|modloader) [ -z "$action" ] || fail "use only one of modloader and --prepare"; action=$a ;;
    -*) fail "unknown option $a (see --help)" ;;
    *)  [ -z "$folder" ] || fail "more than one folder given: $folder, $a"; folder=$a ;;
  esac
done

remember() {
  mkdir -p "$(dirname "$remembered")" && printf '%s\n' "$1" > "$remembered" &&
    echo "Using PD Helper in $1 (remembered for next time)." || true
}

# Downloads PD Helper 4.1 (checksummed) into $1, which must not hold anything else yet (unless $2 = merge)
get_pdhelper() {
  local to=$1 tmp
  if [ -n "$(helper_in "$to")" ]; then [ -n "${2:-}" ] || echo "PD Helper is already in $to."; return; fi
  [ ! -e "$to" ] || [ -n "${2:-}" ] || [ -d "$to" -a -z "$(ls -A "$to" 2>/dev/null)" ] || fail "$to already exists and isn't empty; give another folder: bash $0 --download /path/to/new-folder"
  need curl; need python3; need sha256sum
  echo "Downloading PD Helper 4.1 from github.com/eradication0/PDHelper (about 1.4 MB)..."
  mkdir -p "$to"
  tmp=$(mktemp -d "$to/.download.XXXXXX")
  curl -fsSL -o "$tmp/pdh.zip" "$pdh_url" || { rm -rf "$tmp"; fail "couldn't download $pdh_url"; }
  echo "$pdh_sha256  $tmp/pdh.zip" | sha256sum -c --quiet - || { rm -rf "$tmp"; fail "PD Helper zip checksum mismatch"; }
  python3 -I -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$tmp/pdh.zip" "$to"
  rm -rf "$tmp"
}

# Inside the Flatpak ("flatpak run io.github.snowryze.PhantomDustRebirth pdhelper"): the sandbox sees only the game's
# data, so PD Helper lives on the game's C: drive (the same C:\PDHelper the script run outside the Flatpak copies to)
# and is downloaded there when it's missing; that command is the request, so nothing is asked.
if [ -f /.flatpak-info ]; then
  [ -z "$folder$want" ] || fail "inside the Flatpak PD Helper always runs from the game's C:\\PDHelper. To use a PD Helper
  folder of your own, run this script from a terminal outside the Flatpak:
  bash ~/.var/app/$app/data/wine-uwp/rebirth-kit/pdhelper-linux.sh /path/to/PD-Helper-folder"
  mode=inside
  data=${WINE_UWP_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/wine-uwp}
  [ -f "$data/prefix/.wine-uwp-ready" ] || fail "Phantom Dust isn't set up yet: start it once first."
  host_bin=$(dirname "$(readlink -f "${WINE_UWP:-$app_bin/wine-uwp}")")
  dotnet=$data/prefix/drive_c/dotnet
  pdh=$data/prefix/drive_c/PDHelper
  get_pdhelper "$pdh" merge
  pdh_src=$pdh
  helper=$(helper_in "$pdh")
else
  # PD Helper's folder: the one given, else the remembered one, else this script's own (where older instructions put it),
  # else download it (asked first, unless --download)
  here=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
  if [ -n "$download" ]; then
    to=${folder:-$pdh_default}
    get_pdhelper "$to"
    pdh_src=$(cd "$to" && pwd)
    remember "$pdh_src"
  elif [ -n "$folder" ]; then
    case $folder in *.zip|*.7z|*.rar) fail "$folder is an archive: unzip it first, then give the folder it makes." ;; esac
    [ -d "$folder" ] || fail "no folder $folder"
    [ -n "$(helper_in "$folder")" ] ||
      fail "no PD Helper exe in $folder; give the folder that has \"PD Helper 4.1.exe\" (look one level deeper if you unzipped it)."
    pdh_src=$(cd "$folder" && pwd)
    remember "$pdh_src"
  elif [ -n "$(helper_in "$here")" ]; then
    pdh_src=$here
  elif [ -s "$remembered" ]; then
    pdh_src=$(head -1 "$remembered")
    [ -n "$(helper_in "$pdh_src")" ] ||
      fail "PD Helper is no longer in $pdh_src. Give its new folder:  bash $0 /path/to/PD-Helper-folder
  or download it again:  bash $0 --download"
  elif [ -t 0 ] && [ -t 1 ]; then
    echo "PD Helper isn't set up yet (it doesn't come with Phantom Dust or the Rebirth kit)."
    read -r -p "Download PD Helper 4.1 from github.com/eradication0/PDHelper into $pdh_default? [Y/n] " answer || answer=n
    case $answer in [nN]*) fail "not downloaded. If you already have PD Helper: bash $0 /path/to/PD-Helper-folder" ;; esac
    get_pdhelper "$pdh_default"
    pdh_src=$(cd "$pdh_default" && pwd)
    remember "$pdh_src"
  else
    fail "PD Helper isn't set up yet (it doesn't come with Phantom Dust or the Rebirth kit). Either:
  bash $0 --download                      downloads PD Helper 4.1 (github.com/eradication0/PDHelper) into $pdh_default
  bash $0 /path/to/PD-Helper-folder       uses a PD Helper you unzipped yourself (the folder with \"PD Helper 4.1.exe\")
  This only has to be done once; later runs remember the folder."
  fi
  helper=$(helper_in "$pdh_src")

  # Normal install: prefer a wine-uwp that can load Rebirth's pdnet, as setup-linux.sh does, so both pick the same build
  host_bin= host_data= host_why=
  for c in "${WINE_UWP:-}" "$(command -v wine-uwp 2>/dev/null || true)" ~/.local/opt/wine-uwp/bin/wine-uwp ~/wine-uwp/bin/wine-uwp; do
    [ -n "$c" ] && [ -x "$c" ] || continue
    d=$(dirname "$(readlink -f "$c")")
    [ -x "$d/wine" ] || continue
    if grep -qa WINE_UWP_INJECT "$d/../lib/wine/x86_64-windows/wineuwp-launch.exe" 2>/dev/null; then host_bin=$d; break; fi
    [ -n "$host_bin" ] || host_bin=$d
  done
  if [ -z "$host_bin" ]; then
    host_why="wine-uwp was not found (set WINE_UWP=/path/to/bin/wine-uwp if it's somewhere else)"
  else
    host_data=${WINE_UWP_DATA:-${XDG_DATA_HOME:-$HOME/.local/share}/wine-uwp}
    [ -f "$host_data/prefix/.wine-uwp-ready" ] || host_why="no wine-uwp prefix in $host_data; set up Phantom Dust first"
  fi

  # Flatpak install
  flat_data= flat_why=
  if ! command -v flatpak >/dev/null; then
    flat_why="flatpak is not installed"
  elif ! flatpak info "$app" >/dev/null 2>&1; then
    flat_why="the $app Flatpak is not installed"
  else
    flat_data=${WINE_UWP_DATA:-$HOME/.var/app/$app/data/wine-uwp}
    [ -f "$flat_data/prefix/.wine-uwp-ready" ] || flat_why="no wine-uwp prefix in $flat_data; start Phantom Dust once first"
  fi

  case $want in
    host)    [ -z "$host_why" ] || fail "normal install: $host_why."; mode=host ;;
    flatpak) [ -z "$flat_why" ] || fail "Flatpak: $flat_why."; mode=flatpak ;;
    *)
      if [ -z "$host_why" ]; then
        mode=host
        [ -n "$flat_why" ] || echo "Both a normal install and the Flatpak are set up; using the normal one (add --flatpak for the other)."
      elif [ -z "$flat_why" ]; then
        mode=flatpak
      else
        fail "no Phantom Dust install found.
  normal install: $host_why.
  Flatpak: $flat_why."
      fi ;;
  esac

  if [ "$mode" = host ]; then
    data=$host_data
    dotnet=$data/tools/dotnet-$dotnet_version
    pdh=$pdh_src
  else
    data=$flat_data
    dotnet=$data/prefix/drive_c/dotnet    # seen by wine as C:\dotnet
    pdh=$data/prefix/drive_c/PDHelper     # seen by wine as C:\PDHelper
  fi
fi

if [ ! -f "$dotnet/dotnet.exe" ] || [ ! -d "$dotnet/shared/Microsoft.WindowsDesktop.App" ]; then
  need curl; need python3; need sha512sum
  echo "Downloading the .NET $dotnet_version runtime (once)..."
  tmp=$(mktemp -d "$data/.dotnet.XXXXXX")
  trap 'rm -rf "$tmp"' EXIT
  for f in "${dotnet_files[@]}"; do
    path=${f% *} sum=${f##* }
    curl -fL --progress-bar -o "$tmp/dl.zip" "https://builds.dotnet.microsoft.com/dotnet/$path" || fail "download failed: $path"
    echo "$sum  $tmp/dl.zip" | sha512sum -c --quiet - || fail "checksum mismatch: $path"
    python3 -I -c 'import sys, zipfile; zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' "$tmp/dl.zip" "$tmp/dotnet"
  done
  mkdir -p "$(dirname "$dotnet")"
  rm -rf "$dotnet"
  mv "$tmp/dotnet" "$dotnet"
  rm -rf "$tmp"
  trap - EXIT
fi

if [ "$mode" = flatpak ]; then
  # Copy PD Helper to the C: drive: everything the first time, later only files newer than the copy
  mkdir -p "$pdh"
  cp -ru "$pdh_src"/. "$pdh"/
fi

get_labels() {
  [ -f "$pdh/labels.yaml" ] && return
  need curl; need sha256sum
  curl -fsSL -o "$pdh/labels.yaml.part" "$labels_url" || { rm -f "$pdh/labels.yaml.part"; fail "couldn't download labels.yaml"; }
  echo "$labels_sha256  $pdh/labels.yaml.part" | sha256sum -c --quiet - || { rm -f "$pdh/labels.yaml.part"; fail "labels.yaml checksum mismatch"; }
  mv "$pdh/labels.yaml.part" "$pdh/labels.yaml"
}

# Runs wine with the game's prefix, from PD Helper's folder (PD Helper opens SkillDB.json, Arsenals\ and Mods\ relative
# to the working folder)
run_wine() {
  if [ "$mode" != flatpak ]; then   # normal install, or already inside the Flatpak
    cd "$pdh"
    WINEPREFIX=$data/prefix WINEDEBUG=${WINEDEBUG:--all,err+all} DOTNET_ROOT="Z:${dotnet//\//\\}" exec "$host_bin/wine" "$@"
  fi
  exec flatpak run \
    --env=WINEPREFIX="$data/prefix" --env=WINEDEBUG="${WINEDEBUG:--all,err+all}" --env=DOTNET_ROOT='C:\dotnet' \
    --command=sh "$app" -c 'cd -P "$1" && b=$2 && shift 2 && exec "$b" "$@"' sh "$pdh" "$app_bin/wine" "$@"
}

case $action in
  --prepare) get_labels; [ "$mode" = host ] && echo "Ready (normal install)." || echo "Ready (Flatpak)." ;;
  modloader) get_labels; [ -f "$pdh/ModLoader.exe" ] || fail "no ModLoader.exe in $pdh_src"
             run_wine ModLoader.exe ;;   # its editor crashes without labels.yaml, which the zip lacks
  "")        run_wine "$helper" ;;
esac
