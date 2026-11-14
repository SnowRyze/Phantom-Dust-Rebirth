# Flatpak

Phantom Dust Rebirth for Linux as one file: wine-uwp
([designgears/wine-uwp](https://github.com/designgears/wine-uwp), release v2026.11.14), a Stable kit and a
launcher that runs the kit's `setup-linux.sh`. It works on the Steam Deck, in Game Mode too.

**Nothing of the game or of Windows is in the Flatpak.** The first start gets both on the player's machine:

1. Microsoft's free Windows 11 evaluation ISO is downloaded (about 7 GB) and wine-uwp takes the components it needs
   from it. The ISO is deleted afterwards.
2. Phantom Dust is downloaded from the Microsoft Store (about 10 GB) with the player's own Microsoft account, which
   has to own the game (it is free: <https://apps.microsoft.com/detail/9PCDNBHR11MR>). The sign-in is a link and a
   code, so it can be done on a phone.
3. `setup-linux.sh` sets up pdnet, as it does without the Flatpak.

## For players

```
flatpak install PhantomDust-Rebirth-Linux.flatpak
flatpak run io.github.snowryze.PhantomDustRebirth
```

Started from the app menu or from Steam (add it as a non-Steam game), the setup shows its questions and progress in
windows; started from a terminal, it runs there. On a Steam Deck, do the setup in Desktop Mode (or with a mouse
mapped to the controller): its windows are plain desktop dialogs. Playing afterwards works in Game Mode. About 30 GB of free space are needed during the setup, about 12 GB
afterwards. Everything is kept in `~/.var/app/io.github.snowryze.PhantomDustRebirth/data/wine-uwp`; to skip the ISO
download, put a Windows 11 ISO there as `iso/Windows11.iso` first.

pdnet updates itself at game start as in any Linux setup, so a new Flatpak is only needed for a new wine-uwp.

| | |
|---|---|
| `flatpak run io.github.snowryze.PhantomDustRebirth setup --name NAME` | set up again, or change the name |
| `flatpak run io.github.snowryze.PhantomDustRebirth kit --update` | `setup-linux.sh` with these options (`--remove`, `--help` ...) |
| `flatpak run io.github.snowryze.PhantomDustRebirth stop` | close the game (any other argument goes to wine-uwp) |

Logs: `logs/` in the folder above (`flatpak-setup.log` is the setup's).

## Building

`flatpak/build.sh` builds `out/flatpak/PhantomDust-Rebirth-Linux.flatpak` with the newest Stable kit from the releases, or
with the kit zip given as its argument. It needs `flatpak-builder` and the flathub remote.

The **Flatpak** workflow (`.github/workflows/flatpak.yml`) does the same on GitHub when a Stable release is published
and adds the file to that release; nothing has to be done by hand. For an existing release: Actions > Flatpak > Run
workflow, with the release's tag.

The wine-uwp build is pinned in `io.github.snowryze.PhantomDustRebirth.yml` (URL and SHA-256 of a designgears/wine-uwp
release); change both to move to a newer one.
