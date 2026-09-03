# tearit-store-demo

Demo app-store for **tearit-hq** — one toy + `toy-install.sh`.

## Quick start

Install a toy into your existing tearit-hq desktop:

```sh
curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-store-demo/main/toy-install.sh | sh -s -- hello-toy
```

Then run it:

```sh
sh ~/tearit-hq/@.toys/hello-toy/run.sh
```

## What it does

1. Downloads this repo (`tearitco/tearit-store-demo`, tarball via `curl`,
   falling back to `git clone`).
2. Extracts the named toy to `$PREFIX/@.toys/<toyname>/` (override `PREFIX` to
   install elsewhere).
3. Prints the toy's location and how to run it.

### Env knobs

| var | default | meaning |
|---|---|---|
| `PREFIX` | `$HOME/tearit-hq` | existing tearit-hq install root |
| `STORE_REPO` | `tearitco/tearit-store-demo` | where the store lives |
| `STORE_REF` | `main` | branch / tag / sha |
| `FORCE=1` | — | overwrite an existing toy |

## Included toy

**hello-toy** — prints a banner and the current date. Run it to verify the
download worked.

## Notes

- Toys live under `$PREFIX/@.toys/<toyname>/`.
- A toy is just a directory with a `run.sh` and a `toy.pdl` (metadata).
- Taskbar / manager / cell wiring is **NOT** included yet (future work).
