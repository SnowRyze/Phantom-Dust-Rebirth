#!/usr/bin/env python3
"""Fills in each mod catalog line's newest release: release=<tag> file=<download URL> sha256=<hex>.

Run every 5 minutes by .github/workflows/mod-catalog.yml on the catalog's own branch (mod-catalog, mods.txt), so
players' pdlaunch reads the release from the catalog (raw.githubusercontent.com, no hourly limit) and needs no GitHub
API lookup of its own (launcher/modsync.cpp). The same rule as pdlaunch and the Mod Admin app: the newest release's
first .zip / .7z with a sha256 - a "sha256 <file name> <hex>" line in the release notes (it wins), else the digest
GitHub works out for every uploaded file.

Per mod: the newest release's tag from the website first (github.com/<repo>/releases/latest redirects to .../tag/<tag>;
not the API). The same tag as the line has: left as it is. Otherwise one API lookup (GH_TOKEN). A repo with no usable
release loses its fields (pdlaunch then looks itself and shows why); a lookup that fails keeps what the line had.
Lines are written as the Mod Admin app writes them: id padded, repo, "plugin", then the release fields.

Usage: refresh.py <mods.txt>   (rewrites the file only if something changed; prints what did)
"""
import json
import os
import re
import sys
import urllib.error
import urllib.parse
import urllib.request

AGENT = "rebirth-mod-catalog"
TOKEN = os.environ.get("GH_TOKEN", "")


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *args, **kwargs):
        return None


def latest_tag(repo):
    """The newest release's tag from the website's redirect, or None."""
    opener = urllib.request.build_opener(NoRedirect)
    request = urllib.request.Request(f"https://github.com/{repo}/releases/latest", method="HEAD", headers={"User-Agent": AGENT})
    try:
        opener.open(request, timeout=20)
    except urllib.error.HTTPError as e:
        match = re.search(r"/releases/tag/([^?#]+)", e.headers.get("Location", "") or "")
        if 300 <= e.code < 400 and match:
            return urllib.parse.unquote(match.group(1))
    except Exception:
        pass
    return None


def newest_release(repo):
    """("ok", (tag, url, sha)) / ("none", reason) / ("error", reason)."""
    headers = {"Accept": "application/vnd.github+json", "User-Agent": AGENT, "X-GitHub-Api-Version": "2022-11-28"}
    if TOKEN:
        headers["Authorization"] = f"Bearer {TOKEN}"
    request = urllib.request.Request(f"https://api.github.com/repos/{repo}/releases/latest", headers=headers)
    try:
        with urllib.request.urlopen(request, timeout=20) as response:
            release = json.load(response)
    except urllib.error.HTTPError as e:
        return ("none" if e.code == 404 else "error"), f"GitHub answered {e.code}"
    except Exception as e:
        return "error", str(e)
    body = release.get("body") or ""
    for asset in release.get("assets") or []:
        name = asset.get("name") or ""
        if not re.search(r"\.(zip|7z)$", name, re.IGNORECASE):
            continue
        match = re.search(r"sha256\s+" + re.escape(name) + r"\s+([0-9a-fA-F]{64})", body)
        sha = match.group(1).lower() if match else None
        if not sha and re.fullmatch(r"sha256:[0-9a-fA-F]{64}", asset.get("digest") or ""):
            sha = asset["digest"][7:].lower()
        if sha:
            return "ok", (release.get("tag_name") or "", asset.get("browser_download_url") or "", sha)
    return "none", f"release {release.get('tag_name')} has no .zip / .7z with a sha256"


def main(path):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    lines = text.split("\n")
    entries = [l.split() for l in lines if l.strip() and not l.strip().startswith("#") and len(l.split()) >= 2]
    width = max([len(w[0]) for w in entries] + [14]) + 2
    out, changes = [], []
    for line in lines:
        words = line.split()
        if not line.strip() or line.strip().startswith("#") or len(words) < 2:
            out.append(line)
            continue
        mod_id, repo = words[0], words[1]
        flags = [w for w in words[2:] if "=" not in w]
        fields = dict(w.split("=", 1) for w in words[2:] if "=" in w)
        old = (fields.get("release"), fields.get("file"), fields.get("sha256"))
        new = old if all(old) else None
        tag = latest_tag(repo)
        if not (tag is not None and new and tag == old[0]):
            status, result = newest_release(repo)
            if status == "ok":
                new = result
            elif status == "none":
                new = None
                if all(old):
                    changes.append(f"{mod_id}: release fields removed ({result})")
            else:
                print(f"{mod_id}: lookup failed ({result}) - left as it was")
        if new and (not new[0] or any(c.isspace() for c in new[0] + new[1])):
            new = None  # a tag with spaces can't go in a line
        written = (mod_id.ljust(width) + repo + "".join("  " + f for f in flags)).rstrip()
        if new:
            written += f"  release={new[0]} file={new[1]} sha256={new[2]}"
            if new != old:
                changes.append(f"{mod_id}: {old[0] or 'none'} -> {new[0]}")
        out.append(written)
    result = "\n".join(out)
    if result != text:
        with open(path, "w", encoding="utf-8", newline="\n") as f:
            f.write(result)
    print("\n".join(changes) if changes else "No new releases.")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
