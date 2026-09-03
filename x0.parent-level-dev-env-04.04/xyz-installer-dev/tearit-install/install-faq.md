# install-faq.md — where it goes, what it adds

Plain answers to "what does `install.sh` actually do to my machine?"

## The one command

```sh
curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- tearit-hq
```

The word after `--` (`tearit-hq`) is the **product name**. It decides
the install folder name *and* the command word. Pass something else and
everything follows it:

```sh
… | sh -s -- mydesk      # installs to ~/mydesk, command is `mydesk`
```

---

## Where does it install?

**`$HOME/<product>/`** — so `~/tearit-hq/` by default.

Everything lives in that one folder: the downloaded payload, the
compiled binaries, `bootstrap.sh`, `start.sh`, and the desktop's
config/state under `~/tearit-hq/#.desktop/`. It is fully self-contained
— move it or delete it and nothing else breaks.

Change it with the `PREFIX` env var:

```sh
PREFIX=/opt/tearit-hq   curl … | sh -s -- tearit-hq
PREFIX=~/apps/td        curl … | sh -s -- tearit-hq
```

---

## Does it matter which directory I run the command from?

**No.** `curl … | sh` runs in whatever directory your terminal is in,
but the installer does not write anything to the current directory.

- The download is unpacked in a temporary directory (`mktemp`, deleted
  when the script exits).
- Everything that stays goes under `$HOME` (or `PREFIX` / `BINDIR`).

You can run it from `~`, from `/tmp`, from a USB stick — same result.

---

## Does it add a command word automatically?

**Yes.** It writes a tiny launcher script here:

```
~/.local/bin/<product>          # ~/.local/bin/tearit-hq by default
```

Its entire contents:

```sh
#!/bin/sh
exec sh "$HOME/tearit-hq/start.sh" "$@"
```

So after installing you can run:

```sh
tearit-hq              # start the desktop
tearit-hq stop         # stop it
tearit-hq restart
tearit-hq status
```

### The catch: PATH

`tearit-hq` only works as a bare word if **`~/.local/bin` is on your
`$PATH`**. Most current Linux distros put it there automatically. If
yours doesn't, the installer prints this line for you to add to
`~/.bashrc` (or `~/.zshrc`):

```sh
export PATH="$HOME/.local/bin:$PATH"
```

Until then (or always, if you prefer), this works with no PATH setup:

```sh
sh ~/tearit-hq/start.sh
```

### Moving or skipping the launcher

```sh
BINDIR=~/bin      curl … | sh -s -- tearit-hq   # launcher goes to ~/bin instead
NO_LAUNCHER=1     curl … | sh -s -- tearit-hq   # no launcher written at all
```

---

## What does it touch? What does it NOT touch?

**Touches (all inside your home dir, no root, no `sudo`):**

| Path | What |
|---|---|
| `~/tearit-hq/` | the install (or your `PREFIX`) |
| `~/.local/bin/tearit-hq` | the launcher (or your `BINDIR`, unless `NO_LAUNCHER=1`) |

**Does NOT touch:** system directories, `/usr`, `/etc`, your shell rc
files (it *prints* the PATH line, it doesn't edit anything), other
programs, or any directory you ran it from.

---

## Can I install more than one?

Yes. Each product name is independent:

```sh
curl … | sh -s -- tearit-hq
curl … | sh -s -- tearit-beta
```

gives you `~/tearit-hq/` + `tearit-hq`, and `~/tearit-beta/` +
`tearit-beta`, side by side. Each `start.sh` only manages its own
install (it will not stop the other one).

---

## Uninstall

Any of these:

```sh
tearit-hq uninstall            # stops it, removes the launcher, deletes ~/tearit-hq (asks first)
tearit-hq uninstall -y         # same, no prompt

sh ~/tearit-hq/start.sh uninstall

# or from scratch (launcher already gone, etc.):
curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/uninstall.sh | sh -s -- tearit-hq
```

Or do it by hand — that is the entire footprint:

```sh
rm -rf ~/tearit-hq ~/.local/bin/tearit-hq
```

Adjust the paths if you used `PREFIX` / `BINDIR`. `YES=1` on the
`curl … | sh` form skips the confirmation prompt.

## Reinstall (install / uninstall loop)

```sh
tearit-hq uninstall -y
curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- tearit-hq
```

Or reinstall over the top without removing first: add `FORCE=1` to the
install command.

---

## All the env knobs

| var | default | effect |
|---|---|---|
| *(arg 1)* | `tearit-hq` | product name → folder name + command word |
| `PREFIX` | `$HOME/<product>` | where the desktop is installed |
| `BINDIR` | `$HOME/.local/bin` | where the launcher script goes |
| `PAYLOAD_REPO` | `tearitco/tearit-hq-payload` | which repo to download the desktop from |
| `PAYLOAD_REF` | `main` | branch / tag / commit of the payload |
| `FORCE=1` | — | overwrite an existing non-empty install folder |
| `NO_BUILD=1` | — | download + place files only, don't compile |
| `NO_LAUNCHER=1` | — | don't write the `~/.local/bin` launcher |

## Prerequisites

Linux with an X11 display, plus a compiler (it builds from source):

```
Debian/Ubuntu : sudo apt install build-essential pkg-config libx11-dev libxext-dev libxft-dev curl
Fedora        : sudo dnf install gcc pkgconf-pkg-config libX11-devel libXext-devel libXft-devel curl
Arch          : sudo pacman -S base-devel libx11 libxext libxft curl
```

Windows and macOS are not supported yet.
