# tearit-hq-payload

The curated minimal-desktop payload downloaded by
[`tearit-install`](https://github.com/tearitco/tearit-install).

**Do not edit this repo by hand.** It is generated from the house tree
by `xyz-installer-dev/make-payload.sh`. Hand edits will be overwritten
on the next regen.

## Contents

House subtrees, kept at their original house-relative paths (the C code
resolves everything from one `house_root`):

- `*.monads/*.livedesk-taskbar/` — the taskbar (parser + manager +
  shared render core + helpers)
- `*.monads/*.cursword/` — the cursword entity
- `&.widgits/_shared-lib/` — shared C sources the taskbar build syncs in
- `&.widgits/livedesk-clock/`
- `0.user-pal👤️/00.login-signup/` — the account system (fresh, empty
  `users/`)
- `#.desktop/` — fresh minimal config (USER + clock cells; other header
  cells render but their apps aren't shipped)
- `xyzfs/` — empty guest skeleton

## Use

```sh
sh bootstrap.sh     # compile everything in place
sh start.sh         # launch (scoped to this dir; stop / restart / status)
```
