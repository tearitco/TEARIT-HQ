# How the generator work is done, and the checks to reuse (claude, 2026-11-05)

Owner: "document your process as well. I'd rather you make scripts and harnesses when possible for reuse." House rule: no new `.sh`, no Python. So **reusable means a C verb, a pal program or a harness case file** (`&.widgits/_shared-lib/harness/README.md`). Labels: **OBS** observed this session; **DOC** a rule I follow, not a measurement. Paths are from `44.xyz.01.00/`.

## 1. The toolbox (what exists, OBS)
| Need | Tool | Run |
|---|---|---|
| Restyle one real cell into a Tear-it ramp (alpha never changes) | `@.apps/generator-hq/ops/+x/generator_tile_op.+x restyle <style.pdl> <outdir> <cell.csv>...` | per-cell brightness range |
| Cut real autotile blocks from a whole RPG Maker sheet (`#.NNEST_ASSETS/rmmv-www-img/tilesets/`) | `... shapes <outdir> <sheet.png> <block>` | 48 floor shapes on real pixels |
| One command per whole sheet, per-block brightness (no seams), optional contact sheet per block | `... sheet <map.pdl> <sheet.png> <outdir> [<contactdir>]` | map = `events/gen_restyle/blocks.pdl` |
| Check a finished sheet run | `... verify-sheet <map.pdl> <sheet.png> <outdir>` | re-cuts the raw shapes, checks alpha, non-identity, missing cells; exit 1 on any problem |
| A grid picture for the owner to judge | `... contact <out.png> <cols> <cell.csv>...` | one contact sheet at a time |
| Characters from rows | `@.apps/generator-hq/ops/+x/generator_cast_op.+x cast <cast.pdl> <outdir>` and `cast-check <cast.pdl>` | cast = `events/gen_cast/cast.pdl` |
| The tests | `&.widgits/_shared-lib/harness/generator_tiles.pal` (37), `generator_char.pal` (12), `generator_restyle.pal` (44), `generator_cast.pal` (18) | `cd` to the harness folder, `cpu_gate <house> run <label> -- prisc+x.+x <name>.pal`; verdict in `results/<name>.txt.verdict.txt` |
Styles are data: `events/gen_restyle/tearit-*.pdl` (ramp, accent, outline, seed). A new look is a new row or file, not code.

## 2. The loop for a new sheet or a new character (DOC; this is what I did)
1. **Look first.** Open the source picture (the raw sheet) and decide per block or per character; do not assume. (I assumed A1/A5/A4 sheets were missing and was wrong: I had searched only one folder. Search the repo root, `#.NNEST_ASSETS` included, before saying a file is absent.)
2. **Write the decision as data**: a `BLOCK | sheet | block | style` row, or a `CAST | ...` row. Never hard-code it.
3. **Build through the gate**: `cpu_gate <house> run <label> -- gcc ...`. One heavy job at a time, niced.
4. **Run, then `verify-sheet`** (or the cast harness). The verb checks every cell, not a sample.
5. **Open pictures with my own eyes**, a contact sheet at least per new kind of terrain or character. Say how many I opened and how many I did not.
6. **Add a harness case with a negative** for anything new: a check I have never seen fail is a hope. Every new rule above has one (restyled differs from the original; the alpha check fails on a different shape; the seam check fails on the per-cell path; verify fails on missing cells and on a copied original; a bad row exits 2).
7. **Re-run all four suites**, then commit by explicit pathspec (`commit_scoped.+x`, never `git add -A`), fast-forward `grok` and `main`, push.
8. **Copies for the owner** go to `XO/31.Nov/nov-5/` (PNG and docs only). Nothing goes into a repo `good/` folder without the owner's written approval of the pattern.
9. **Update the handoff** (`16.game/HANDOFF-GENERATOR-GROK-WORK-AND-INTERIM-ASSETS-2026-11-05.md`) with what was built, what was checked, and what was not.

## 3. Mistakes I made today, so the next agent does not repeat them (OBS)
- **Per-cell brightness scaling gave seams** between shapes of one block. Found on a contact sheet, proven by a same-colour check (3 inconsistent with the old path, 0 with block scaling). Now `generator_restyle` case `seams`.
- **576 lines of output killed the verb** in the harness with SIGPIPE (exit 141). A whole-sheet command prints one summary line.
- **Stale expectations** after widening a map (`blocks=12` became 32; a "no map row" negative used a sheet that now had rows).
- **Wrong house path** (one `..` short) gave exit 127 twice. Use the absolute house path.
- **Miscounts I announced**: "25 blocks changed" was 19; "5 colours" was 4 (I had cut the wrong columns). Count with the tool, then quote the tool.
- **`cpu_gate` exit 3** means refused (die temperature 88 C against a limit of 85 C once, a concurrent heavy job another time). It is not a test result and not a build failure: wait and retry, never bypass. I did this with a retry loop in my shell; **that loop is not committed and is the one repeated thing I have not turned into a tool**. Proposal (not done, not my file): a `--wait <seconds>` option on `cpu_gate`.
- **A watcher that matched its own words**: "NOT-RUNNING" contains "RUNNING". Test a wait loop's condition on both states.

## 4. Numbers to quote only from a tool
`verify-sheet` prints `checked / changed_alpha / identical_to_original / skipped_transparent / missing_or_wrong / result`. The harness verdict prints `passed / failed`. Three `EXPECT_FILE` rows print no PASS line, so the count of PASS lines is smaller than the count of rows; say which.

## 5. What is still manual
Choosing a style per block (by eye, one ramp), opening the pictures, and the gate wait. Caps for `generator-hq/out/` and `#.NNEST_ASSETS/generator-hq/interim/` do not exist yet (AGENTS.md runtime-file rule): committed sizes today are about 4 MB of contact sheets plus the small cast PNGs; the cell folders are rebuilt, not committed.
