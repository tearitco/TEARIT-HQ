# 🔒 Phase 1 report — lock what we built (2026-11-04)

Personal note for the owner. Phase 1 = give every fix of today a test that can fail, and spread the shared pieces. Status: **done except one gap (marked ⚠️)**.

## ✅ What now exists

| 🧪 Check | What it proves | How to run | Result |
|---|---|---|---|
| `place_and_move`, `lamps_at_dusk`, `mouse_align` | click-place, lamps on at dusk/off at dawn (brightness measured), mouse→ray→cell round trip | `python3 &.widgits/_shared-lib/presentation/run_suite.py` | ✅ OK (ran 13:3x, editor window) |
| `dropdown_thumb` (new) | the book dropdown hides the sidebar scrollbar thumb: closed list shows a 117 px bright run (control), open dropdown 10 px | same suite | ✅ OK + negative test failed as it should |
| `clock_roundtrip.py` (new) | save → advance 3 h → load puts the game clock back, in every skin; control step fails if the advance did nothing | `python3 …/presentation/clock_roundtrip.py` | ✅ 10 OK, 2 SKIP (cdda-hq, civ-rpgce never started a clock) |
| `lamps_rollout.py` (new) | buy lamp → `lamps on` → LIGHT row at the lamp's cell → `lamps off` → row gone, in every skin with a 3D book | `python3 …/presentation/lamps_rollout.py` | ✅ 11 of 11 (rpg-pvp-hq has no 3D book) |
| `placer_verb` (new) | the shared placer places the armed tile with the `voxel` verb | suite | ✅ OK on the new build |

## 🧩 Rolled out / made shareable
- 💡 **Lamps** are now in `petslike_core.c` (`lamps` verb + `lamps_export_room`), opt-in by an `<app>/lamps.pdl`. Lamp item + lamps.pdl added to 11 skins; dusk/dawn pages turn them on/off where a skin has phase pages (played end to end on rpg-pet). The lamp art is still the placeholder pillar.
- 🖌️ **Brush placer** is now a shared text-include, `petslike_placer.c` (like `petslike_life.c`): the editor calls four hooks. Only the editor uses it today, by your call; the header lists how another skin opts in.
- 🕒 **Clock on load** fix verified in all skins that have a clock.

## 🐞 Things found and fixed on the way
- The harness left the **tile brush armed** after a run (it shortened the tile list and hid the thumb) → it now restores `brush.txt`.
- My first clock test advanced each clock 1 min before saving → clocks restored from backups / set back; the final run changes nothing.
- A failed `cd` once unpacked backups into the presentation folder (nothing live touched; cleaned).
- CPU: local TTS hit 135 % / 98 °C (guard ACT) → narration now runs on debil; incident reviewed, alert cleared.

## ⚠️ Gap — read this
The editor's **daemon is still the old build** (it started 3.5 h ago). The click-and-Enter path (`place_and_move`, `mouse_align`) therefore ran on the OLD placer code; only the verb path was exercised on the new module. The permission layer refused my kill of that daemon. To close it: `! kill 1070866`, then `board_ctl recover '@.apps/rpg-edit-hq'`, then run the suite. Until then the refactor is "built and verb-tested", not "fully proven".

Also: the editor board stack I launched (≈ 37 % of a core idle) is still open; close it when you are not testing.

## ❓ Decisions for you
1. Restart the editor daemon now (above) so Phase 1 can be signed off?
2. Real lamp tile art instead of the placeholder pillar? (pick from the RPG Maker sheets together)
3. Next: Phase 2 (pets chatting window) or Phase 3 (hand-editable weights)? My pick: Phase 2.

## 🔗 External references (for other AI agents)
- Repo: `&.widgits/_shared-lib/presentation/{present_harness.py,run_suite.py,clock_roundtrip.py,lamps_rollout.py,scenarios/}`, `&.widgits/_shared-lib/{petslike_core.c,petslike_placer.c,petslike_life.c}`
- Book: `10-user-docs/PRESENTATION-VIDEO-PIPELINE.md`, `16.game/UNIVERSE-PATHS-PETS-DSR-AI-CHAIN-2026-11-04.md`, `09-appendix/FINAL-REPORT-2026-11-03-04-EDITOR-STANDARDS-HAI-AND-CPU-NIGHT.md`
- Commits: 6d122cfa3, 47fc292ac, 475656994, 28edc343c, cbf9e0a0b (branch `claude`)
- Outside: RPG Maker MV tilesets (https://www.rpgmakerweb.com/products/rpg-maker-mv), Kokoro TTS (https://github.com/hexgrad/kokoro), Cataclysm: DDA tiles (https://github.com/CleverRaven/Cataclysm-DDA)
