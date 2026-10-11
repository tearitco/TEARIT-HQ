# desk-restart

`desk_restart` restarts a house desktop (`button.sh reset`) and never leaves it silently down.
Origin: 2026-10-07, a remote `reset` over a non-login ssh shell killed the desktop, the rebuild failed
(`pkg-config` is in /usr/local/bin, not on the non-login PATH) and nobody was told for minutes.

Build: `sh ops/build_desk_restart.sh` -> `ops/+x/desk_restart.+x` (git-ignored). POSIX, Linux + macOS, no system()/popen.

## Usage
    desk_restart.+x --house <root> [--button <button.sh>] [--display <D>] [--xauthority <F>]
                    [--path-extra <dir[:dir]>] [--wait <sec, 90>] [--dry-run] [--shell </bin/bash>] [--require <tool>]...

1. PREFLIGHT (nothing is killed): house + button.sh exist, build tools named in the taskbar build scripts
   (pkg-config, make, a C compiler) plus every `--require` resolve in the LOGIN shell the child will use,
   a DISPLAY exists when the house has a taskbar. Failure: `RESTART|REFUSED|<reason>`, exit 3.
2. RUN: detached child (setsid, stdin /dev/null) `<shell> -lc 'bash button.sh reset'`, output appended to
   `<house>/#.desktop/desk_restart.log`. `--path-extra`, DISPLAY and XAUTHORITY are re-applied inside the login shell.
3. WAIT: child exited AND a `khtpm_taskbar_manager` of this house is running.

## Verdicts / exit codes
- `RESTART|UP|pid=<mgr>|seconds=<n>` exit 0 (also: dry-run preflight ok)
- exit 2 usage error; exit 3 `RESTART|REFUSED|<reason>`
- `RESTART|DOWN|build-failed|child-exit-<n>|no-taskbar-after-wait|log=<path>` exit 4, then a loud banner and the last 5 log lines (and build_error.log)
- `RESTART|TIMEOUT|...` exit 5 (child still running after --wait; it is left running)

## Over ssh (login shell, so the user's PATH applies)
    ssh host 'bash -lc "<house>/../../../&.widgits/desk-restart/ops/+x/desk_restart.+x --house <house> --display :0 --path-extra /usr/local/bin"'

Check first with `--dry-run`. Verify the op: pal harness `_shared-lib/harness/desk_restart.pal` (41 checks on scratch fake houses).
