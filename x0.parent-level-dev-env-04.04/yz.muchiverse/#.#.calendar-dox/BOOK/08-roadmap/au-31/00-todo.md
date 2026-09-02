# au-31 — real todo list, 2026-08-31 (network HQ windows + compliance)

Real, dated work list for today's remaining scope. Written per SKILLS.md
§5 ("write real findings into real docs as you go") - this is the
concrete next-step list, not a status snapshot.

## Context (why this list exists)

Investigating "why does class=\"db-hq\" show unrelated Actor/RPG data
instead of my own content" led to a real finding: `dbhq_load_actors()`
in `khtpm_entity_menu_render.c` reads a real PDL file
(`&.widgits/db-hq/data/actors.pdl`) directly from INSIDE the shared
parser/renderer file, rather than through a separate manager process
the way db-hq's own Common Events tab already does
(`khtpm_hq_manager.c`). Not hardcoded string literals (real file-based
data, compliant with the house's core rule) - but the loading logic
itself sitting in the shared file is a real drift risk: every mode
that needs its own data ends up adding its own inline loader to the
SAME shared, "hard boundary" file, each with its own slightly
different assumptions about file format/paths, and nobody owns
verifying they stay consistent.

Direct instruction: "we need to factor out those managers first or
they will create reference drift" - before building the 3 new network
HQ window shells (irc-chat-hq/forum-hq/chain-hq), the RIGHT-shaped,
fully-compliant pattern is a real manager process per app (matching
Common Events' own real precedent), not more inline loaders bolted
onto `khtpm_entity_menu_render.c`.

## Real todo, in order

1. **Design the real per-app manager shape** - DONE, see
   `01-manager-design.md`: irc-chat-hq's real manager designed in full
   (state files, action-file contract, exact real op it shells out to)
   against `khtpm_hq_manager.c`'s own proven precedent; forum-hq/
   chain-hq to follow the identical pattern once irc-chat-hq is proven
   live (not designed in per-field detail yet, same real shape applies).
2. **Wire the class-detection + `<module src="...">` launch** for the
   3 new classes (irc-chat-window/forum-window/chain-window) in
   `khtpm_entity_menu_render.c` - the generic sidebar+panel injection
   point, gated on the new manager's published state file existing,
   NOT a copy of `dbhq_load_actors()`'s own inline-loader shape. This
   IS a real edit to the shared "hard boundary" file - do it carefully,
   one class at a time, verify live after each.
3. **Build/verify each manager + window pair one app at a time** (IRC
   first - real precedent already exists in its own real schema/ops,
   see NETWORK-CELL-HQ-WINDOWS-DESIGN.md §12), not all 3 at once -
   confirm the real shell+manager split actually works live before
   repeating it for Forum/Chain.
4. **Retire the 3 placeholder `.chtpm`/`.css`/launcher files already
   created today** (`&.hq-apps/irc-chat-hq/`, `forum-hq/`, `chain-hq/`)
   once the real manager-backed versions replace them - they were
   built against the WRONG assumption (that a bare `.chtpm` alone,
   with no manager, could render a real sidebar+panel shell) before
   this finding, and are not the real, final shape.
5. **`HOUSE_CODE_PITFALLS.md`** - add a new entry on this exact class
   of mistake (see below, done same pass as this list).
6. **Audit pass** (after 1-4 above land, not before) - a real sweep of
   `khtpm_entity_menu_render.c` and any other manager/ops files under
   the livedesk-taskbar house for OTHER inline data-loading functions
   that should have been a separate manager process from the start
   (`dbhq_load_actors()` is the one already found - there may be
   siblings for Classes/Skills/Items/etc. or in other `.c` files this
   session hasn't looked at yet). Real, scoped goal: a list of exactly
   which loaders are inline-in-shared-file vs real-separate-manager,
   not a rewrite of everything found - decide case by case whether
   each one is worth splitting out.

## Explicitly not started yet

Nothing in this list has real code written against it yet - this is
the plan, written down before touching `khtpm_entity_menu_render.c`
again, per direct instruction to factor out the manager shape first.
