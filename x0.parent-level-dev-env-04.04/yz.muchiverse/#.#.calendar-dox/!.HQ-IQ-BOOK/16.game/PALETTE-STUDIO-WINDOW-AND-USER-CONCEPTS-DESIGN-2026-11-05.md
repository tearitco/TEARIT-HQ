# Palette-studio window, user concepts and live weights: design (2026-11-05)

Labels: **SAID** owner; **READ** in a named file; **OBS** measured or opened today; **DOC** my reasoning, not run. Nothing in sections 3 to 5 is built. Builds on decision D33 (steps 1 to 3 are done: `generator_voxel_op objsheet`, `generator_ground_op`, `palette_studio_op`) and on the Concept Bank (D32).

## 1. What the owner asked (SAID, 2026-11-05)
"a palette-studio where we can choose common rpg-maker template types (sprite/dungeon/etc) and each type will have knobs that can be turned for entire sheet or selected areas, and sheet or selected areas can be saved as sheet or crop" (done headless). Then: "yes, and cli-io input for user concepts / weights, which should be most valued? (and be able to be tied to other existing, etc and see how sliding them changes output gen live) etc, do design doc if u need". "Yes" answered my question: put the window in the existing **palettes widget**, not a standalone app.

## 2. Facts this design stands on
| Fact | Source |
|---|---|
| Render latency: object sheet **84 ms**, ground sheet **303 ms**, object crop plus selection **35 ms** (avg of 5, weak CPU, no browser load measured) | OBS `palette_studio_op render` |
| The renderer has **no image tag and no slider element**. It has `<canvas sprite="${canvas_raw}"/>` (a raw RGBA file, repainted; a canvas switches the window to a **30 fps tick**), `<bar value max>` (progress, not input), `<button>`/`<item>` with action=, and `<cli_io id target_id label action>` | READ `khtpm_core_render.c` (grep of tags, lines 6150 to 6385, 9279), `layout-studio/CATALOG.md`; rpg-pet already uses `<canvas id="room" sprite="${canvas_raw}"/>` |
| `<cli_io>` rules: one line typed field, `target_id` = state key in `cli_io_state.txt`, `action=` runs on Enter, a field must open **empty** (the manager must not trust stale state), `content=` seeds the buffer, put real content in `content=` never `label=` | READ `layout-studio/CATALOG.md`, `khtpm-house-standards` skill |
| Concept Bank shape: `NODE`, `KIND master|spoke`, `SLOT n | POINTS_TO=<node> | WEIGHT=<float>`; a master can point at other masters (force to motion 0.70); **weights live in exactly one place** (the pointing slot); the master's mirror table is derived; edits are bounded `spoke_weight_delta` records (|delta| at most 0.2) checked by `concept_edit_validate`; `new_concept_node` is **named but not implemented** in the validator; auto-promotion OFF, promotion is a person's act | READ `concept-bank/data/masters/force.pdl`, `ops/concept_edit_validate.c`, `AUTO-PROMOTION-RULE.md` |
| The palettes widget exists (`&.widgits/palettes`, xhtpm files per palette); its stub names a planned **generate** row (user-pallet, cdda, df, kenney, paint, generate) | READ `palettes-stub.xhtpm` line 3 (**not read in full**) |
| Visible effect of each existing knob at weight 0.5, whole sheet, against the neutral sheet (compare-png): **objects:** `big` changed 58.6% of pixels, mean colour shift 29.3, luminance correlation 0.488 (a structural change); `warm` 91.2% / shift 7.1; `metal` 91.1% / shift 7.0; `worn` 17.5% / 4.1; `round` 3.1% / 1.3. **ground:** `wet` 100% / shift 12.6; `warm` 70.6% / 2.5; `grain` 24.3% / 4.1; `edge` 5.2% / 0.9; `wear` 4.0% / 1.2 | OBS (measured today, one sheet each) |

## 3. Design
### 3.1 Placement: a "generate" tab in the palettes widget (DOC, recommended)
Reason: the widget is already where a person picks tiles; the generated sheet is what the palette then offers, so generate and pick sit together. The studio window calls the four `palette_studio_op` commands and owns no generator logic. **Before building:** read the palettes widget source and its stub in full (not done), `khtpm-house-standards`, `CENTROID_GOLD_STD.md`, `TPMOS-COMPLIANCE-DEBT.md`; check for an existing sibling layout that already shows a list plus preview (the 2026-09-06 incident: do not add a new `layout_*` branch to the shared renderer when the generic swatch-grid or panel path already fits).

### 3.2 Window layout (DOC, sketch)
```
 [template v: outside_a2]   [sheet | crop]  [save]   status: REPLAY ok / error line
 +-- concepts (sorted by weight, highest first) ---+   +-- preview (canvas) ------------+
 | 1 cozy      0.80  [-][+]  ties: warm .6 wet .2 |   |                                |
 | 2 rusty     0.45  [-][+]  ties: wear .7 warm .3|   |   live render of the sheet     |
 | 3 (knob) wet 0.30 [-][+]                       |   |   or the selected area         |
 | selection: blocks 0,0 .. 3,0   [clear]         |   |                                |
 +-- command: [ cli_io: new cozy | tie cozy warm .6 ] +--------------------------------+
```
### 3.3 Live preview (DOC)
- The window never renders. A small **manager** op owns the session (template, concept weights, selection) as plain files under the studio state folder; a change appends one line to a **marker file** (append-only, size growth = change, not mtime; house rule); the manager sees the growth, calls `palette_studio_op render` with the current options into `preview.png`, converts it to `preview.raw` (RGBA) and the window shows it with `<canvas sprite="${canvas_raw}"/>`.
- **Debounce:** one render at a time; if more changes arrive during a render, render once more with the latest state (so a held button never queues 30 renders). Latencies above (35 to 300 ms) are inside a comfortable live range.
- **Cost (house rule: our programs run alongside a browser on a weak CPU):** idle = the manager sleeps on the marker file, no polling render; **a canvas forces the window's 30 fps tick (OBS in the renderer source), so measure the idle CPU with the window open before shipping** (`cpu_gate`, measured with a browser running); fallback if too hot: draw the preview as `<item sprite=DIR>` (sprite.csv, no tick) or open the canvas only while a knob changed in the last few seconds. The preview crop and selection shrink render time (35 ms for a crop).

### 3.4 "Sliding" without a slider (DOC)
There is no slider element and the shared renderer must not get a new per-project branch. Use what exists: for each concept or knob a **[-] [+] button pair** (step 0.05, key repeat while held where the generic input supports it) and a **`<bar>`** showing the value; an exact value is typed in the cli_io (`set cozy 0.72`). A true slider would be a generic renderer feature for every app (file a request in `TPMOS-COMPLIANCE-DEBT.md`, do not hide it inside the studio).

### 3.5 User concepts and ties (DOC; the bank's own shape)
- A **user concept is a master node** in a *user bank* (same file format as `data/masters/*.pdl`, name prefix `concept_`). Its **ties are `SLOT | n | POINTS_TO=<existing master or knob master or another concept> | WEIGHT=<-1..1>`**: this is exactly how `force` points at `motion` (0.70). So "tied to other existing" is the bank's native master-to-master pointer, no new mechanism. Example `concept_cozy.pdl`: `SLOT | 0 | POINTS_TO=look_warm | WEIGHT=0.60`, `SLOT | 1 | POINTS_TO=ground_warm | WEIGHT=0.50`, `SLOT | 2 | POINTS_TO=ground_wet | WEIGHT=0.20`, `SLOT | 3 | POINTS_TO=concept_rustic | WEIGHT=0.30`.
- **Session weight** (how strongly the concept is applied right now, 0..1, the slider) is a studio option, delta-style like `knob=`: new option `concept=<name>:<weight>` on `render`, `save`, `replay`, recorded in the `.studio.pdl` like every other option, so a replay reproduces it.
- **Resolution (the maths):** effective knob k = clamp( sum over concepts c of weight_c x strength(c to k), range of k ), where strength follows ties transitively (depth at most 3; a cycle is refused with the path in the error). Knobs not tied by any concept are untouched. A template only uses the knob masters it has (objects: `look_*`; ground: `ground_*`), so one concept can carry both families and each template takes its half. Sliders on raw knobs stay (`knob=`); a raw knob is just a concept with one tie of strength 1.
- Effective knobs are **added to the Concept Bank spoke weights** (the thing's own weights, D32), then clamped by the master's range, the same order the ops use for `knob=`.

### 3.6 "Which should be most valued?" (DOC, with the evidence in section 2)
Three separate meanings; the design handles each:
1. **Order in the window:** concepts are listed by weight, highest first, so the ones you value most are on top (a view, no stored rank; one authority per fact).
2. **Conflicts:** two concepts pulling one knob in opposite directions. Default **sum** (they cancel partly, like physics); an optional per-session **winner** mode takes only the single largest |weight x strength| per knob, so "the most valued concept wins". Recommendation: sum by default, winner as a toggle.
3. **Which knobs deserve the most attention:** by observed effect (section 2): on objects `big` (structure), then `warm` and `metal` (broad colour), then `worn`; `round` only matters on boxy things; on ground `wet` first, then `warm` and `grain`; `edge` and `wear` are detail. **Recommendation:** starter concepts should tie mostly to the high-effect knobs (`big`/`warm`/`metal`, `wet`/`warm`/`grain`) with small strengths on the detail knobs, so a single slider always visibly moves the picture.
The owner may still rank by taste: nothing here fixes a priority list; it is data (weights) the person edits.

### 3.7 The cli_io command line (DOC)
One field, `target_id=ps-cmd`, opens empty, Enter runs `palette_studio_op concept <command>` and clears it; the result line goes to the status row. Grammar (every command validates before writing; a bad one changes nothing and prints one error line):
`new <name>` create a concept (empty ties) | `tie <name> <target> <strength>` add or change a tie (target must exist: a knob master or another concept) | `untie <name> <target>` | `set <name> <weight>` session weight | `drop <name>` | `rank` print the sorted list | `winner on|off` | `save sheet|crop <name>` (calls `palette_studio_op save`) | `promote <name>` writes a **proposal** file for a person (never into the shared bank).
Names are `[a-z0-9_]` up to 24 characters; weights 0..1, strengths -1..1; an edit of an existing tie larger than 0.2 per command is refused (the validator's bound; `set` and typing a new tie are exempt because they are session input, not bank edits).

### 3.8 Files, caps and who may write where (DOC)
- **User concepts are runtime data, not code:** under the studio state folder (gitignored), never `xyzfs/users`, never the shared `concept-bank/data/masters` (promotion is a person's act, through `concept_edit_validate`; `new_concept_node` is not implemented there, so a promoted concept needs that validator type built first: a gap, recorded).
- **Caps (house rule "runtime files must have an enforced cap"):** at most 64 concepts, 8 ties per concept, 2 KB per concept file, one session file under 8 KB, the marker file trimmed at 64 KB, `preview.*` overwritten in place (2 files). Enforced in the op (refuse over the cap), second guard = `cpu_guard` `hygiene_sweep()`; add a row to `RUNTIME-FILE-GROWTH-STANDARD.md` in the commit that adds the writer, with files and bytes per hour.
- Every process the window starts (the manager) is registered through the shell funnel or `proc_reg_op` and exits when its owner is gone.

### 3.9 Design-choice test (D34, PROPOSED)
| Question | Answer |
|---|---|
| One authority per fact | knob effects only in the generator ops; concept ties and weights only in concept files (bank format); the window holds nothing |
| Modular | window = layout + cli_io + marker files; manager = fork/exec of `palette_studio_op`; concepts = data rows |
| Scalable | the 10th concept or template is a file or a row; a third template kind is one branch in the studio op |
| Long term | matches the attrition docs: the bank is the personality, hub-and-spoke, weights in one place, bounded edits, a person promotes; the same files can later receive learner proposals |
| House shapes | pdl rows, C ops with fork/exec, marker files, khtpm window through the generic renderer, a harness that can fail, capped runtime files |
| Cost and risk | medium; risks: canvas 30 fps tick (measure), a missing slider element (use buttons now, file a renderer request), concept cycles (refused), too many renders (debounce) |

## 4. Build order, each step gated by a harness case that can fail (DOC)
1. **Concept files and the resolver** in `palette_studio_op`: `concept=<name>:<weight>` option, `winner` mode, depth and cycle checks, caps. Gate: a concept with two ties equals the two `knob=` deltas byte for byte; a cycle, a missing target and an over-cap file are refused; replay of a record that uses a concept reproduces the hash.
2. **Concept commands** headless: `palette_studio_op concept new|tie|untie|set|drop|rank` (the cli_io grammar). Gate: every command and every refusal.
3. **Manager op and marker files**: debounced re-render, `preview.raw`. Gate: ten quick changes produce at most two renders and the final preview equals a direct render of the final state.
4. **The "generate" tab** (xhtpm + css) in the palettes widget. Drive it through the per-pid relay (not xdotool); before/after PNGs of the real window plus the click path; one picture per step.
5. **Idle and active cost** measured with a browser running (`cpu_gate`, canvas on and off); the commit states the numbers.
6. Promotion proposals and the `new_concept_node` validator type (separate decision).

## 5. Open questions for the owner
1. Is "most valued" the on-screen order, the conflict rule (sum or winner), or a fixed priority list per template? (The design does the first two and leaves the third as data.) 2. Should user concepts be shared by all desks or per user? (Recommend per desk session now, sharing = a promotion.) 3. May a true slider be requested as a generic renderer feature, or do you want the plus and minus buttons only? 4. Which starter concepts do you want first (suggestion: cozy, rusty, frozen, royal, overgrown)?

## External references
Repo: `@.apps/generator-hq/ops/{palette_studio_op,generator_voxel_op,generator_ground_op}.c`, `events/gen_studio/templates.pdl`, `&.widgits/concept-bank/` (masters, spokes, `ops/concept_edit_validate.c`, `AUTO-PROMOTION-RULE.md`), `&.widgits/palettes/` (`palettes-stub.xhtpm`), `@.apps/layout-studio/` (CATALOG.md, README.md), `_.monads/_.livedesk-taskbar/ops/khtpm_core_render.c`, decision log D26 to D33, `ROBOT-CHAT-BLUEPRINT.md` (bank as personality, template and delta), XO `10.kilo-attrition/03-ATTRITION-VIA-CHAT.md`. Outside: none used.
