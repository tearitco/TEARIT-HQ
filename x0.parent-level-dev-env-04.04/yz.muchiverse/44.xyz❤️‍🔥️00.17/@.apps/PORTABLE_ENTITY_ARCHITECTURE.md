# PORTABLE_ENTITY_ARCHITECTURE.md — Cross-Game Entity-as-Directory Design

Written 2026-08-02, direct instruction. Applies to `civ-txt`, `tactics-txt`, and any future project in this family — a shared entity model so a unit/city/creature can, in principle, be dropped from one game's own directory tree into another's and still work.

**Real precedent, not invented here**: `❤️‍🔥️!.world_architecture+1=rusindol.txt` (dated 2026-03-18, status "Active Direction"), a real house architecture doc describing a general container-piece model. Quoted directly (§2 of that doc): *"Any piece can itself be a container... Pieces may contain pieces (inventory, interiors, equipment groups, nested storage)."* This document applies that real, already-decided direction concretely to civ-txt/tactics-txt, and extends it with a possession→methods mechanic that — confirmed via direct code research this session — **does not exist anywhere in this house's real code yet**. We are setting that second part's precedent here, not following one.

---

## 1. What's real precedent vs. what's new

**Real, already-decided (from the world_architecture doc)**:
- Directory containment IS the location relationship — a piece's parent directory is where it "is." Moving a piece's directory between containers moves it in the world, full stop, no separate "position" bookkeeping needed for containment (though `pos_x`/`pos_y` still matters for pieces that are *directly* in a map container, for on-map placement — see §3).
- Any piece can contain pieces: inventory, vehicle occupancy, interiors are all the SAME mechanic (a child directory), not three different systems.
- Global/registry data (item definitions, templates) is explicitly NOT a world piece — only *instances* live in world/map containers.

**Confirmed via direct code research this session, NOT existing precedent — new work**:
- Mutaclysm's own `possessed_id` mechanic is a **hero-only self-control toggle** (`ops/choice.c:1194-1195`, only ever set to `"none"` or `"hero"`). Its own comment states outright: *"items, monsters — never possessable today."*
- Monsters/NPCs/items in mutaclysm have **no `piece.pdl` at all** — only a flat `state.txt`. There is nothing for them to "show" as methods even if possession did target them.
- `active_target_id` (the variable that actually drives `${piece_methods}` — `chtpm_parser_pal.c`'s `load_dynamic_methods()`) is a real, working, GENERIC mechanism (it can resolve `${piece_methods}` for any given piece_id) — but it is never, anywhere in this house's current code, retargeted away from `hero`/`selector`/`loader`. The plumbing to make "select an entity → that entity's own real methods show up" exists at the engine level; nothing has ever wired it to a non-hero target.

**Conclusion**: the container/nesting half of what you described is real, decided house direction — build on it directly, don't re-derive it. The possession-shows-entity-methods half is a coherent, buildable extension of an already-working generic mechanism (`active_target_id`), but genuinely new — nobody has built it before, so we get to set the real shape here, carefully, matching the existing engine's own actual capabilities rather than inventing something the engine can't actually do.

---

## 2. The entity directory shape (per instance)

```
<container>/<entity_id>/
├── state.txt              # core stats - universal fields + game-specific fields, same file
├── piece.pdl               # NEW - this entity's own real METHOD table (its available actions)
├── skills/                 # NEW - abilities/methods this entity can use, structured (see §4)
├── inventory/               # only present if this entity CAN carry things
│   └── <item_id>/
│       ├── state.txt
│       └── piece.pdl        # an item can have its own methods too (use/equip/etc) - same mechanic
└── ai/                     # only present if this entity is AI-controlled (reuses the REAL,
    └── state.txt            # already-proven decision_mode chassis from my-chara-txt/
                              # 014.wsr-pal💸️📌️+2/ops/corp_decide.c - 0=preset/1=weighted/
                              # 2=rl-stub/3=llm/4=human - not reinvented, directly reused)
```

**Vehicles are not a fourth mechanic** — a vehicle IS an entity (its own directory, its own `state.txt`, possibly its own `piece.pdl` for "drive"/"exit" actions), and an entity entering a vehicle is exactly the same operation as an entity entering a container/inventory: **move the entity's own directory to become a child of the vehicle's own directory** (e.g. `world_01/civs/rome/units/legion_3/` → `world_01/civs/rome/units/wagon_1/passengers/legion_3/`). No special-cased "vehicle occupancy" field needed — containment already means this.

**"Moved at the directory level, moves on the map"** — stated precisely: for an entity living DIRECTLY in a map/world container (not inside another entity's own inventory/vehicle), its `pos_x`/`pos_y` in `state.txt` is still the real on-map coordinate (matching civ-txt's/tactics-txt's own current, already-correct convention) — directory containment tells you WHICH map/container it's in, `pos_x`/`pos_y` tells you WHERE within that container. Moving to a different container (a different map, or into another entity) is a real directory move (`mv`/`rename()`), not just an edited field.

---

## 3. Universal minimal schema — the actual portability contract

For a directory to be genuinely draggable between civ-txt and tactics-txt (or any future project) and still mean something, a SMALL set of `state.txt` fields need consistent names/semantics across every project that opts in. Everything else in the same file stays project-specific (extra key=value lines are always silently ignorable by any reader that doesn't know them — this is the existing, real, plain key=value convention already used everywhere in this house, no format change needed):

```
entity_type=<civ_unit|civ_city|tactics_unit|monster|npc|item|vehicle|...>
hp=<int>                  # present if the entity can take damage/die at all
pos_x=, pos_y=             # present if directly located in a map/world container
owner_id=                 # present if the entity belongs to a player/civ/side
```

**Open design question, not resolved here — see §6.** Whether this should be a hard, validated contract or just a soft convention is genuinely undecided; flagged for you.

---

## 4. The possession → methods mechanic, concretely

1. A selector/xlector (in board-viewer's own case: the widget's own selector cursor, per `@.apps/BOARD_WIDGET_ARCHITECTURE.md` §5's already-planned `SELECT_TILE`/command-bus hookup) targets an entity.
2. The HOST game (civ-txt/tactics-txt, not the widget itself — the widget has no game-rule awareness by design, same "genuinely shared, project-agnostic" principle `chtpm_rgb_render.c` already follows) receives that selection and sets its OWN `active_target_id` to the selected entity's own `piece_id` — a real, already-supported engine operation, just never exercised on a non-hero target before.
3. `${piece_methods}` — already fully generic — resolves and shows THAT entity's own `piece.pdl` METHOD rows as real, numbered, dispatchable actions, automatically, no new engine code needed for this part.
4. Deselecting (or the turn/context ending) resets `active_target_id` back to whatever the host's own default is (`hero`/a menu root/etc.).

**This is why every entity needs its own `piece.pdl` now** (§2) — without one, `${piece_methods}` for that target correctly shows `[No Methods]` (the engine's own existing, graceful empty state), same as it does today for any piece with no METHOD rows.

---

## 5. What this means for civ-txt / tactics-txt specifically, right now

**Not a retroactive change to current P1 code.** `civ-txt`'s `config.txt` (flat `city_count`) and `tactics-txt`'s `units.txt` (flat roster, no positions) are P1 scope and stay as-is until their own real P2 work (cities, movement) actually begins — this doc is the target SHAPE for that P2+ work, not something to migrate existing working code toward today.

**Both design docs already got the container-nesting instinct right** (`civ-txt`'s own `civs/<civ_id>/cities/<city_id>/state.txt`, `tactics-txt`'s own `battle_01/units/<unit_id>/state.txt`) — good foundations, cross-referenced from both docs now (see the pointer added to each). What's missing in both, to be added when P2 actually starts: a real `piece.pdl` per city/unit, the universal schema fields (§3), and (for tactics-txt specifically) genuine inventory-as-subdirectory instead of any flat `equipped_item_ids` list currently sketched.

**`CIV_TXT_DESIGN.md` and `TACTICS_TXT_DESIGN.md` have each had a pointer to this doc added** to their own §3 (data model) sections — read this doc in full before building either project's own P2.

---

## 5a. Real precedent for actually moving an entity between windows — `101.drag-drop-test=ON🀄️`

A real, working, already-proven mechanism exists for exactly "drag an entity from one game's own window to another's" (`01.muchi-pals-🥚️-13.01/system/egg_window.c` + `ops/pet_export.c`/`pet_import.c`) — confirms §1's "moved not copied" principle concretely, and answers open question 1 below with a real, working shape rather than a guess:

1. **The source moves its own real piece directory FIRST**, synchronously, via a real `rename()` (`pet_export.c:228-230`, own comment: *"The pet is moved (not copied) to the exchange directory"*) — into a **shared sibling `exchange/` directory**, not directly into the target's own tree.
2. **Only THEN does it hand off a tiny payload** — literally just the entity's own id, one line, nothing else (`egg_window.c:279-292`: `fprintf(f, "%s\n", pet_id)`). No path, no bytes, no directory reference crosses the window boundary.
3. **The target looks up the real data itself**, by fixed convention (the same sibling `exchange/<id>/` location), never from anything in the payload (`pet_import.c:37-44`).
4. **Window-to-window only** — dropping onto bare desktop (no target window found under the cursor) is NOT handled, confirmed no such branch exists.
5. **Real, hard-learned CPU lesson**: real XDND (the X11 native drag protocol) was tried and abandoned — window-manager reparenting broke self-lookup, and an unthrottled idle-poll loop caused "the exact CPU-spin bug that crashed the machine once already" (`BOARD_WIDGET_ARCHITECTURE.md:152-165`). The working replacement polls on a real, throttled timer (`usleep(16000)`, `gl_mirror.c:368`) — never poll unthrottled for this kind of cross-window handoff.
6. **Explicitly out of scope even there**: "re-pairing" (switching a live target's whole data root via drop) is documented as undesigned, no real precedent — only fixed-target content import (drop something INTO an already-running, already-paired target) is proven. Board-viewer's own drag-and-drop re-pairing plan (`BOARD_WIDGET_ARCHITECTURE.md` §8) inherits this same real limitation.

**Applied to entities**: dragging a unit/city between civ-txt and tactics-txt (or onto the desktop as a "card") would follow the exact same shape — move the real entity directory into a shared `exchange/` root, hand off only the entity_id, let the target resolve the rest by convention. Real desktop-drop (no target game window) has no precedent and would be new work if ever wanted.

## 6. Open questions — genuinely undecided, asked rather than guessed

1. **Hard contract or soft convention?** Should the universal schema (§3) be validated somewhere (a real check that rejects/warns on a malformed dropped-in entity), or is "just don't break the field names, extra fields are fine" sufficient for now, given no actual drag-and-drop-a-directory-between-games tooling exists yet either?
2. **Does the possession/`active_target_id`-retargeting mechanism get built now**, as part of board-viewer's own upcoming entity-rendering work (a natural, real integration point), or is this purely a design doc for later, with no code yet — given civ-txt/tactics-txt don't have real placeable entities at all yet, there's nothing to select/possess in either project today?
3. **Item methods** (§2's `inventory/<item_id>/piece.pdl`) — do you want items to have their own real methods too (use/equip/drop as numbered options when an item itself is selected), or should items stay pure data for now, with "use item" being one of the CARRYING entity's own methods instead (simpler, less recursion)?
