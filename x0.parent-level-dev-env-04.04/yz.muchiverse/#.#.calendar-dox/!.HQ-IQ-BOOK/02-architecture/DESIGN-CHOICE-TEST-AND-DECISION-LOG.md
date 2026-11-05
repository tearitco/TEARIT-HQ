# Design-choice test and decision log (owner 2026-11-05: "which is more modular / scalable, long-term wise and spiritually aligned with the best practices of the house? ALWAYS ASK THAT. DOC IT")

Every time there are two or more ways to build something, ask these questions BEFORE picking, write the answer down, and put the decision in the log below. A fast stopgap is allowed only when it is labelled as one with its removal condition.

## The test (ask in this order)

1. **One authority per fact?** Is there already a file or field that says this? Adding a second one for the same fact makes every reader pick or merge (DSR bridge rule: one authority per fact, everything else a projection).
2. **Modular?** Does the change stay in data (`.pdl` rows) or in one small shared piece, or does it add per-project knowledge to a shared file (the shared renderer, the TPMOS parser)? Per-project branches in shared code are the thing the house keeps removing.
3. **Scalable?** What happens at the 10th consumer, the 10th kind of entity, the 2nd board? Does a new reader have to remember a special case?
4. **Long term?** Which way does the roadmap point (pieces with one uid, "all characters playable", one scene for 2D and 3D, the in-memory DB)? Prefer the choice that is still right after that lands; if the other is chosen, name the migration.
5. **Matches the house shapes?** Pure text-include for shared pure logic, op + IPC for stateful, data in `.pdl`, marker files not mtime, a harness case that can fail (pal + case + C, no Python), explicit cap on anything written at runtime.
6. **Cost and risk of the better choice?** Heavy-collision files, a contract change, a migration. Say it, and say how a harness case guards it.

Output: a recommendation and the one reason that decides it, then a log entry.

## Decision log

### 2026-11-05 D1: how a PET (or Harold) chosen by Enter shows as "possessed" (HUD, look, Esc-first)

Facts (verified): the xelector's `possessed_id` is written by the ENGINE (`bv_menu_input.c`) and means "arrows drive this PIECE". HUD `poss`, the lighter box, and Esc-first (`esc_first.txt`) all read it. Pets are NOT pieces: they are numbered map events (`pieces/1..17`, footers) whose state is game state (`state/pet_<id>.txt`); the daemon chooses them (`possess_at_xelector`) and keeps `active.txt` + `flag_ctl` + `flag_follow`. Result today: a chosen pet leaves `possessed_id=none`, so the box stays free-looking, HUD says `poss -`, and Esc leaves In. This breaks the owner rule that ALL possession is controlling.

Options: (A) a second field `controlled_id` written by the daemon, readers take whichever is set. (B) the daemon writes the pet id into `possessed_id`, and the engine treats an id with no `pieces/<id>/state.txt` as externally driven: it holds the possession, moves nothing, and `9` / Esc-first releases it; the daemon mirrors the id into `active`/`ctl`/`follow` and reacts when it goes back to `none`.

Test: (1) A makes two authorities for "who is the xelector controlling"; B keeps one. (2) A needs every reader (2D, 3D, HUD, Esc-first, future ones) to union two fields; B changes none of them. (3) A's 10th reader repeats the union; B's costs nothing. (4) The roadmap makes pets ordinary entities/pieces ("piece = one uid", "all characters playable", Harold is not special): then A's second field is dead weight and needs a migration, B's contract ("possessed_id names whatever is controlled") is already the end state. (5) B is a small contract extension plus data, testable with the scratch input op already used by `pieces_2d_parity` and the scratch daemon method (`possess_enter_daemon`). (6) B's cost: a small edit in `bv_menu_input.c` (the heavy-collision file) and a release coupling in the daemon; guard with a negative harness case (a missing piece file must not move or crash anything).

DECISION: B. A is acceptable only as a labelled stopgap if B is blocked; its removal condition would be "pets are pieces".
Build order for B: (1) harness case for the engine contract (id without a piece file: Enter-set possession holds, arrows do not move a piece, `9` releases), (2) the engine change, (3) the daemon writes the id on Enter and clears it on release (and reads `none` back as "xelector detached"), (4) re-run `pieces_2d_parity`, `possess_enter*`, `esc_first_*`, then a live frame (HUD `poss <pet>`, paler box, Esc releases then leaves In).
STATUS: decided and documented, not built.
