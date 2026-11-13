#!/usr/bin/env bash
# Builds the Flatpak: out/flatpak/PhantomDust-Rebirth-Linux.flatpak (a single file, installed with "flatpak install FILE").
#
#   flatpak/build.sh              with the newest Stable kit from this repository's GitHub releases
#   flatpak/build.sh KIT.zip      with this kit zip (a full kit: pdnet/ with eos.ini in it)
#
# Needs flatpak and flatpak-builder, and the flathub remote for the runtime (org.freedesktop.Platform 25.08).
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/.." && pwd)
app=io.github.snowryze.PhantomDustRebirth
repo=${PDNET_REPO:-SnowRyze/Phantom-Dust-Rebirth}
out=$root/out/flatpak
mkdir -p "$out"

if [ $# -ge 1 ]; then
    cp -f "$1" "$here/kit.zip"
else
    url=$(curl -fsSL -H 'Accept: application/vnd.github+json' "https://api.github.com/repos/$repo/releases/latest" |
        python3 -I -c 'import json, re, sys
for a in json.load(sys.stdin).get("assets", []):
    if re.fullmatch(r"PhantomDust-Rebirth-Stable-\d+(-PC)?\.zip", a["name"]):
        print(a["browser_download_url"])')
    [ -n "$url" ] || { echo "no PhantomDust-Rebirth-Stable-<N>-PC.zip in the latest release of $repo" >&2; exit 1; }
    echo "Kit: $url"
    curl -fL -o "$here/kit.zip" "$url"
fi

flatpak-builder --user --force-clean --disable-rofiles-fuse --install-deps-from=flathub \
    --state-dir="$out/state" --repo="$out/repo" "$out/build" "$here/$app.yml"
flatpak build-bundle --runtime-repo=https://dl.flathub.org/repo/flathub.flatpakrepo \
    "$out/repo" "$out/PhantomDust-Rebirth-Linux.flatpak" "$app"
echo "Built $out/PhantomDust-Rebirth-Linux.flatpak"
