# DB-HQ-HOUSE — per-tab layout design (khtpm + house nav)

**Written:** 2026-08-28
**Audience:** the next agent implementing stub db-hq tabs
**Supersedes (this queue only):** HANDOFF.md “do Terms first” and OPEN-2do Task 2
“list-via-sidebar-injection for all 14 stubs.” Terms and Common Events are
**out of scope** until the human signs off after seeing **Actors**.

Visual MV reference (layout only): `rpg-maker-database.html` in this directory.
Live window: `&.hq-apps/db-hq/dashboard.chtpm` + `dashboard.css`, rendered by
`khtpm_entity_menu_render.c` (`g_is_db_hq`).

---

## 0. Standing rules

1. **Do not touch Terms or Common Events** this pass — no manager, no state
   file, no sidebar injection, no `dbhq_tab_is_real()` lines for those two
   except as they already exist. Do not restyle CE’s embedded event editor.
2. **House nav, not HTML-only.** Numbered badges via `dbhq_nav_take()` /
   `assign_generic_onclick_nav()`. Protocol (`CREATOR_AGENT.md` §2.5):
   **content 1..N first, close LAST.** Digit jump + Enter + arrows.
   Agents drive `db_hq_history.txt` / relay first (`_.0.aigent-testing-k9.txt`).
3. **khtpm layout, not a new widget kit.** `.chtpm` tags + `.css` + existing
   `Elem` tree. CSS filename is derived from the chtpm path. Dark house
   palette already in `dashboard.css` (`#141414` / `#1a1a1a` / `#eeeeee`) —
   keep it; do not revert to the light MV mock.
4. **TPMOS:** real manager binary + published state file. No bash-`printf`
   chtpm for tab logic (`TPMOS-COMPLIANCE-DEBT.md`). Renderer injects lists;
   it does not scan JSON itself.
5. **Superficial first.** Visible MV-shaped chrome + stub rows. Not full CRUD,
   not parameter-curve editors, not trait pickers.
6. **One tab as the proof.** Implement **Actors** only after this doc. Other
   stub tabs stay `(coming soon)` until the human likes Actors.

---

## 1. Bug: default tab vs nav `[1]` (fix first, independent of Actors UI)

**Symptom:** launch db-hq → nav `[1]` is **Actors** (first `<tab>`), but the
body is **Common Events**.

**Cause (two sources, same lie):**

| Site | Current | Should be |
|---|---|---|
| `khtpm_entity_menu_render.c` `g_dbhq_current_tab` | `DB_HQ_COMMON_EVENTS_TAB` (11) | `0` (Actors) — first tab, same as MV and as nav 1 |
| `dashboard.chtpm` | `<tab label="Common Events" class="active"/>` | `class="active"` on **Actors** |

Tabbar nav is assigned in chtpm order, so `[1]=Actors` always. Content
followed `g_dbhq_current_tab`, which skipped to CE because that was the only
“real” tab. **Wrong:** focus and painted tab must match.

**Fix:** default C + chtpm to Actors. Until Actors has a manager, the body
is the existing placeholder `"Actors — (coming soon)"`. CE still works when
that tab is selected. Do **not** keep defaulting to CE “because it’s the
only real tab.”

---

## 2. Shared layout family (all list tabs)

MV Database (everything except System / Types / Terms): **left ID list +
right settings workspace**. House mapping:

```
<window class="database-window db-hq">
  <tabbar>  …15 tabs, MV order…  </tabbar>
  <sidebar>  numbered ID rows  </sidebar>
  <panel>    settings blocks for the selected row  </panel>
</window>
```

**Nav ring (list tabs, once a tab is real):**

1. Tabs in bar order (`[1]` Actors … `[15]` Terms).
2. Sidebar rows (`0001: Name` …) then `+ Add` / Change Maximum if present.
3. Panel fields that have `onClick=` (text fields, buttons). Labels that are
   display-only are **not** numbered.
4. Close, last.

**State file (per tab, published by that tab’s manager):**

```
# db_hq_actors.state.txt  — one record per line
id=1	name=Harold
id=2	name=Therese
```

Empty names still occupy a slot (`id=5	name=` → `0005:`). Renderer formats
`%04d: %s` for the sidebar label.

**chtpm:** keep **one** dashboard.chtpm. Do not compose 15 files. Tab switch
clears/rebuilds sidebar+panel from the active tab’s state. Placeholder tabs
keep today’s `dbhq_render_placeholder_tab()`.

**`dbhq_tab_is_real()`:** add **only** the tab being implemented (Actors
next). Terms and CE already true — leave those two lines.

---

## 3. Per-tab designs

MV order. **Status:** `leave` = do not implement this pass; `stub-next` =
Actors is the first build; `placeholder` = still `(coming soon)` after
Actors until approved.

### 3.1 Actors — **first implementation** (`stub-next`)

**MV:** sidebar of actor IDs; workspace = General / Graphics / Initial
Equipment / Parameters / Traits / Note.

**House superficial (Actors v1):**

| Region | khtpm | Nav? |
|---|---|---|
| Sidebar | `<item>` rows `0001: Harold` | yes |
| `+ Add Actor` | last sidebar item, `input:` name like CE’s add (copy mechanism, do not change CE) | yes |
| Panel title | `Actor` + selected id | no |
| Name | `<cli_io>` or labeled text + `input:` | yes |
| Nickname | same | yes |
| Class | labeled text (string stub, not a live Classes join yet) | yes |
| Initial / Max level | labeled numbers | yes |
| Profile | one text line | yes |
| Face / Character / Battler | three labeled tiles; sprite.csv if we have a face, else dashed box + filename stub | yes (opens nothing v1, or `input:` filename) |
| Equipment slots | Weapon / Shield / Head / Body / Accessory as labeled strings | yes |
| Parameters | eight rows `MHP/MMP/ATK/DEF/MAT/MDF/AGI/LUK` as **label + number**, not fake CSS bars v1 (bars later if wanted) | numbers yes if editable |
| Traits | read-only list box, may be empty | no unless `+`/`−` exist |
| Note | one text line | yes |

**Data:** house **PDL only** (`SECTION | KEY | VALUE`). Never JSON.
Canonical file: `&.widgits/db-hq/data/actors.pdl` (`ACTOR | key | value`
records). Manager publishes a copy to `#.desktop/db_hq_actors.state.txt`
(same PDL). Seeds 4 named stubs (Harold / Therese / Marsha / Lucius) +
empty slots through 0012.

**Manager:** `actors_hq_manager.c` + `build_actors_hq_manager.sh`, same
fork/execl pattern as Terms — but **do not copy Terms’ “dump lines into
the CE sidebar loader.”** Actors gets its **own** load/inject functions
(`dbhq_load_actors` / `dbhq_inject_actors_sidebar`). Reusing
`dbhq_load_common_events()` for Actors is how Terms went wrong.

**Window size:** sidebar `scaled(210)` already; panel fills the rest.
Two visual columns inside the panel via nested `<row>` flex if CSS flex
is enough; if v1 is cramped, **single stacked column** is acceptable
(house first, MV second).

### 3.2 Classes (`live` 2026-08-28 — inject, PDL)

Same shell as Actors: `dbhq_inject_list_sidebar` / `dbhq_inject_list_panel`.
Data: `&.widgits/db-hq/data/classes.pdl` (`CLASS | key | value`).

### 3.3 Skills (`live` 2026-08-28 — inject, PDL)

`skills.pdl` (`SKILL`). Panel: Name, stype, mp/tp cost, scope, occasion,
description.

### 3.4 Items (`live` 2026-08-28 — inject, PDL)

`items.pdl` (`ITEM`). Name, itype, price, consumable, scope, description.

Sidebar item IDs; panel Name, Item type, Price, Consumable, Scope,
Description, Effects stub, Note.

### 3.5 Weapons (`live` 2026-08-28 — inject, PDL)

`weapons.pdl` (`WEAPON`). Name, wtype, price, atk, def, description.

### 3.6 Armors (`live` 2026-08-28 — inject, PDL)

`armors.pdl` (`ARMOR`). Name, atype, etype, price, def, description.

### 3.7 Enemies (`live` 2026-08-28 — inject, PDL)

`enemies.pdl` (`ENEMY`). Name, battler, mhp, atk, def, exp, gold.

### 3.8 Troops (`live` 2026-08-28 — inject, PDL)

`troops.pdl` (`TROOP`). Name, member1–3 stubs. No battle-test in v1.

### 3.9 States (`live` 2026-08-28 — inject, PDL)

`states.pdl` (`STATE`). Name, restriction, auto_remove, note.

### 3.10 Animations (`live` 2026-08-28 — inject, PDL)

`animations.pdl` (`ANIMATION`). Name, graphic, frames. No cell grid in v1.

### 3.11 Tilesets (`live` 2026-08-28 — inject, PDL)

`tilesets.pdl` (`TILESET`). Name, mode, a1/a2/b filenames. No tile picker.

### 3.12 Common Events — **LEAVE ALONE**

Already real: manager, sidebar, embedded command editor, Play, Add Command.
Do not restyle to “match Actors.” Do not change default-away behavior
except the launch default in §1.

### 3.13 System (`live` 2026-08-28 — inject, PDL)

`system.pdl` (`SYSTEM`). Superficial groups as sidebar rows (Game / Party /
Music / Options), fields on the panel. Not a fake actor-style ID list of
games.

### 3.14 Types (`live` 2026-08-28 — inject, PDL)

`types.pdl` (`TYPE`). Sidebar is the five MV lists (Elements, Skill Types,
Weapon Types, Armor Types, Equipment Types); panel is the slot names.

### 3.15 Terms — **LEAVE ALONE**

Already a “real” tab but **wrong layout family** (CE sidebar reuse).
Human wants to see Actors done house-style **before** deciding whether
to rebuild Terms as a field grid. No Terms renderer/manager edits.

---

## 4. Key decisions

| Decision | Why |
|---|---|
| Default tab = Actors (index 0), matching nav `[1]` | Human-reported bug; MV also opens on Actors |
| One chtpm, swap sidebar/panel in C | Matches live db-hq; 15 composed files would fight CSS derivation |
| Own loader per list tab, never CE’s `g_dbhq_events[]` | Terms already proved that reuse looks like the wrong product |
| Superficial Actors before other stubs | Human will judge house-vs-MV on one screen |
| Dark `dashboard.css` stays | House standard; MV mock is light only as a *structure* reference |
| PDL only, never JSON | House database language is `SECTION \| KEY \| VALUE` |
| System/Types/Terms are a different layout family | Don’t fake a sidebar of IDs on those tabs |

---

## 5. Implementation order (after this doc)

0. **Bugfix (this session):** default tab + chtpm `active` → Actors.
1. Human reads this file.
2. **Actors v1** (manager + inject + panel fields + house nav).
3. Human reviews Actors live.
4. Only then: Classes → … → Tilesets (copy Actors shell).
5. System / Types after that family is agreed.
6. Terms rebuild **only if** the human says so after Actors.

---

## 6. Files an Actors PR would touch (not this session except §1)

- `&.hq-apps/db-hq/dashboard.chtpm` — active class on Actors (§1 now)
- `*.monads/*.livedesk-taskbar/ops/khtpm_entity_menu_render.c` —
  default tab (§1 now); later Actors load/inject/panel (not CE/Terms)
- NEW `actors_hq_manager.c` + `build_actors_hq_manager.sh`
- `#.desktop/db_hq_actors.state.txt` (published)
- This file, if Actors v1 forces a layout correction
