# `prisc+x` string opcodes

Added 2026-09-03. **Backwards-compatible**: the `s*` mnemonics are new
`strcmp()` branches in `parse_line()` and new cases in the executor;
`sregs[16][4096]` is a separate zero-initialised bank no other opcode
touches; the new `OpBase` enum values are appended so every prior value
is unchanged. A `.pal` that never uses an `s*` mnemonic runs exactly as
before, and a project that never re-syncs this file is unaffected.

They exist so a `.pal` can act as a **UI projector** — read line content
out of state files, format it, and write a `key=value` text file — which
the integer-only `ecall` set could not do.

## String registers

`s0` .. `s31`, 4096 bytes each, zero on start. Distinct from `x0`..`x15`.

## Opcodes

| form | effect |
|---|---|
| `slit sD, "text"` | `sD = "text"` (literal from source) |
| `scpy sD, sS` | `sD = sS` |
| `sappend sD, sS` | `sD = sD . sS` |
| `sgetenv sD, "NAME"` | `sD = getenv("NAME")` (empty if unset) |
| `sfmt sD, "fmt", a, a, …` | printf-style: `%d` consumes the next arg as an `xN` int reg, `%s` consumes the next as an `sN` string reg, `%%` → `%`. Other text copied. |
| `sread sD, xLINE, sPATH` | `sD =` line number `xLINE` (0-indexed, newline stripped) of the file named by `sPATH`. Sets `x12` = length, or `-1` if the line/file is missing. |
| `ssplit sSRC, "sep", sBEFORE, sAFTER` | split `sSRC` on the first `"sep"`; `x12` = 1 if found else 0 (then `sAFTER` is empty). Safe if operands alias. |
| `sfind xD, sS, "needle"` | `xD` = byte index of `"needle"` in `sS`, or `-1` |
| `slen xD, sS` | `xD = strlen(sS)` |
| `sfopen xD, sPATH` | open `sPATH` for write (truncate); `xD` = fd `0..7`, or `-1` |
| `sfappend xD, sPATH` | open `sPATH` for append |
| `swrite xFD, sS` | write `sS` + `"\n"` to fd `xFD` |
| `sfclose xFD` | close fd `xFD` |
| `sbeq sA, sB, label` | branch to `label` if `sA == sB` |
| `sbne sA, sB, label` | branch to `label` if `sA != sB` |
| `strim sD` | strip leading/trailing whitespace from `sD` in place |
| `satoi xD, sS` | `xD = atoi(sS)` (string register -> int register) |

`sfopen`/`sfappend`/`swrite`/`sfclose` share the same 8-slot fd table
as the integer `ecall` file ops, so don't mix more than 8 open at once.

## Projector idiom

```
sgetenv s15, "KHTPM_PKG"
sfmt   s14, "%s/db_hq_actors.state.txt", s15     # input
sfmt   s13, "%s/state/ui.txt", s15               # output
sfopen x9, s13

read_pos x2, "..."            # (optional) integer count from a state file, or:
li x2, 0
li x3, 20                     # cap
loop:
beq x2, x3, done
sread s0, x2, s14
slen x8, s0
beq x8, x0, done             # EOF: sread left s0 empty and x12=-1
ssplit s0, "|", s1, s2       # id | name
sfmt s5, "actor_%d_id=%s", x2, s1
swrite x9, s5
sfmt s5, "actor_%d_name=%s", x2, s2
swrite x9, s5
addi x2, x2, 1
j loop
done:
sfmt s5, "actors_count=%d", x2
swrite x9, s5
sfclose x9
```

The static `.chtpm` then does the layout with `${actor_N_name}` +
`<repeat count="${actors_count}" bind="actor">`.
