# Why muchi-pals shares one module across screens (not a manager per
screen, the way 102.agy-txt's file browser does)

> ## SUPERSEDED 2026-07-31 - READ THIS FIRST
> §1-6 below were written before checking real TPMOS precedent, and
> concluded per-screen modules were unnecessary. That conclusion is
> WRONG and reversed - see `05-jul30-live-bugs-and-plan.md` and
> `06-per-screen-module-standard.md` for the real story: TPMOS's own
> `orchestrator.c` (confirmed by direct read of `/home/no/Downloads/
> 1.TPMOS_c_+rmmp.0100.0110/pieces/chtpm/plugins/orchestrator.c`)
> already splits shared infrastructure (keyboard/render/parser threads)
> into one always-alive orchestrator process, separate from whichever
> `<module>` is currently running - but nothing in this house had ever
> actually given each SCREEN its own distinct module the way the real
> `launch_module()` liveness-check mechanism supports. Direct
> instruction (2026-07-31): "that is the true standard i wanted... we
> will test that here before doing it elsewhere." Prototyped on
> `pets.chtpm` first, confirmed safe via direct `debug.txt` evidence
> (real kill+fork+exec exactly on genuine navigation, correct reuse
> while staying on one screen, no orphaned processes, first-frame
> render correct), then rolled out to all six of this project's own
> screens (`main_module.pal`, `user_module.pal`, `faucet_module.pal`,
> `store_module.pal`, `pets_module.pal`, `processes_module.pal`) -
> live-tested working. **This IS now the real, intended house standard
> going forward - every project with more than one real screen should
> eventually get this same per-screen module split**, starting with
> whichever project is worked on next (see `06-per-screen-module-
> standard.md` for the full house-wide scope). §1-6 below are kept
> only as a record of the (wrong) reasoning that preceded this, and of
> the real, separate, still-valid finding in §4 (dead `eggpal.chtpm`,
> already deleted) and §3.1's audit method (still useful for judging
> whether a screen needs dedicated OPS too, a separate question from
> the module-process split).

Written 2026-07-30, direct question from the user during the j30
house-wide button/href refactor session (see `#.haiku+/refactor-list-
j30.txt`, `#.haiku+/short-summary-j30.txt`): "they all use the same
module... did u feel the need to refactor that to have them use their
own module/managers? ... do they still use ops?" This doc is the
answer, grounded in a real read of every layout + compose op in this
project, for future agents (and future me) to check against instead of
re-deriving from scratch.

## 1. Short answer (SUPERSEDED - see banner above)

No refactor was needed, and doing one would have gone AGAINST this
project's own documented, already-corrected house standard - not
fixed a bug. Two separate things explain why, plus one real, small,
unrelated finding turned up along the way (§4).

## 2. Why ONE shared module, not a manager per screen

Every real, reachable layout in this project (`main.chtpm`, `user.
chtpm`, `faucet.chtpm`, `store.chtpm`, `pets.chtpm`, `processes.
chtpm`) declares the exact same `<module>system/prisc+x pal/
main_loop_chtpm.pal</module>` - ONE persistent PAL loop process,
shared across every screen, with chtpm's own real `href` doing screen
switching (never the module restarting). This is not an oversight -
it's the corrected house standard for pure discrete-menu games,
recorded directly in `#.haiku+/!.xyzos-standards+1.txt` §16
("INTERACT MODE IS ONLY FOR CONTINUOUS, FREE-FORM KEY RELAY... A PURE
DISCRETE MENU SHOULD NEVER USE onClick="INTERACT", USE
${piece_methods} INSTEAD"):

> REAL MISTAKE, CORRECTED (egg-pals/wsr-pal, 2026-07-18): an earlier
> pass copied mutaclsym's own `<button label="Control Hero"
> onClick="INTERACT" />` gate pattern wholesale into egg-pals
> ("Control Pets")... This was overzealous copy-paste... a needless
> manual "engage" step before ANY menu option could be selected, for a
> project where every action is already a single discrete choice.

`main_loop_chtpm.pal`'s own header comment confirms this project
already lived through that correction and moved past it: "Supersedes
this project's own earlier interact+module/'Control Pets' version
(sec.16 retired that pattern for pure discrete-menu games like this
one, in favor of ${piece_methods})."

So: a real map/cursor project (mutaclysm - actual continuous movement)
legitimately needs a dedicated `INTERACT`-gated module. A pure
discrete-menu project (this one, pal-chain, pal-forum, pal-chat-irc,
login-signup) does not - one shared PAL loop + real `${piece_methods}`
buttons IS the correct, intended shape, confirmed against real
`chtpm_parser.c` dispatch code (nav-mode clicks already relay through
without `INTERACT` ever being needed).

## 3. Why agy-txt/file-menu got a DIFFERENT treatment (a dedicated
   native-C manager) - a different problem, not a stricter standard

`${piece_methods}` only knows about method names declared ahead of
time in a `.pal` file. It cannot express "one clickable row per file
that happens to exist in this folder right now" or "results filtered
by whatever's currently typed into a search box" - genuinely dynamic,
runtime-variable content. Agy-txt's and file-menu's browsers needed
exactly that (arbitrary directory listings, live search), so they
needed a persistent daemon (`manager/+x/agy_browser_manager.+x`) that
polls input itself and writes real `<button>` markup into an
injection point (`${directory_browser_markup}`) every time the
listing changes.

Muchi-pals has no screen with that requirement - checked directly
(§3.1 below).

### 3.1 Direct audit of every `${panel_content}` injection point in
    this project (the muchi-pals equivalent of agy-txt's
    `${directory_browser_markup}`)

Grepped every layout for `${...}` placeholders, then read
`ops/muchi_compose_frame.c` (the op that fills `${panel_content}`)
line by line for every screen that uses it:

| screen      | `${panel_content}` actually contains          | selectable? |
|-------------|------------------------------------------------|-------------|
| user        | name, token count                               | no - status only |
| faucet      | token count                                     | no - status only |
| store       | token count                                     | no - status only |
| pets        | one selected pet's live stats (HP/MP/hunger/etc)| no - status only |
| processes   | a list of tracked pet-window PIDs + alive/dead  | no - status only |

None of these rows are meant to be clicked or selected - they're
read-only status text, the same category as mutaclysm's own
`${game_map}` (a real rendered viewport, correctly a text blob) or
pal-chain's own wallet/balance/mining-status lines - NOT the PITFALL
65 pattern (a fake, hand-tracked `[>]` cursor standing in for what
should be real, clickable, focus-managed buttons). Every screen here
that DOES have real selectable actions (faucet, pets, store) gets them
from `${piece_methods}` - real, chtpm-native, auto-generated buttons -
sitting right next to `${panel_content}` in the same layout. There is
no hidden dynamic-markup need in this project that would justify a
dedicated manager the way agy-txt's did.

## 4. Real, small, unrelated finding along the way: `eggpal.chtpm` is
   dead code

While auditing every layout (§3.1), one file didn't fit the pattern:
`pieces/chtpm/layouts/eggpal.chtpm` still has the OLD, retired shape
`#2` describes - `<module>${module_path}</module>` (a literal,
NEVER-substituted placeholder string - there is no live chtpm
variable-substitution mechanism that sets `module_path`; the only
`module_path=` in this project is an unrelated plain bash variable
inside `button.sh`, used only for launching, not for `${}` layout
substitution) and `<button label="Control Pets" onClick="INTERACT"
/>` - exactly the pre-§16-correction pattern this project's own
`main_loop_chtpm.pal` header says was already retired.

Confirmed via direct grep: nothing hrefs to `eggpal.chtpm` from any
other layout, and `button.sh` never launches it either - the real
entry point is `main.chtpm`, which correctly has no `INTERACT` button
at all. `eggpal.chtpm` is unreachable, orphaned, and would not even
render correctly if it somehow were reached (`${module_path}` would
show up as literal, unsubstituted text). Not touched this session -
flagging for a future cleanup pass, not fixing now since it's
unreachable and therefore zero real risk.

## 5. Do they still use ops? Yes - that's this family's actual
   reusability layer

Every individual action in this project is still its own small,
single-purpose, independently-compiled op binary: `buy_egg.c`,
`claim_tokens.c`, `feed_pet.c`, `train_pet.c`, `clean_pet.c`,
`toggle_sleep.c`, `hatch_egg.c`, `generate_egg.c`, `tick_pets.c`,
`list_processes.c`, `coin_flip.c`, `destroy_card.c`, `export_card.c`,
plus `muchi_menu_input.c`/`muchi_compose_frame.c` for dispatch/render.
The ONE shared PAL loop invokes whichever op a button click resolves
to; the ops themselves stay small, focused, and swappable - this IS
the reusability the manager-per-screen pattern also aims for, just
applied at the op layer instead of the module layer. Same principle
(pal-chain: `chain_send.c`/`chain_balance.c`/`chain_miner.c`; pal-
forum, pal-chat-irc, login-signup all follow the identical shape).

## 6. Bottom line

- Shared module across screens: correct, matches this project's own
  documented §16 correction, not something to undo.
- Dedicated managers (agy-txt/file-menu style): solve a different,
  specific problem (runtime-dynamic markup generation) this project
  genuinely doesn't have anywhere, confirmed by direct read of every
  screen's own compose logic.
- Ops: still the real unit of reusability here, unchanged, heavily
  used.
- One real, minor, unrelated finding: `eggpal.chtpm` is dead, orphaned
  pre-§16 code - safe to leave alone, worth deleting in a future
  cleanup pass.
