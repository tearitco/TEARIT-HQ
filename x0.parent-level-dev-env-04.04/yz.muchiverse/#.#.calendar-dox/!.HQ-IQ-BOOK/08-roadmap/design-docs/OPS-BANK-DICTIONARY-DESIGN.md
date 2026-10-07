# Ops dictionary as a Concept Bank ("banks with weights")

Written 2026-10-07 (claude). **Design only, nothing built.** Owner questions/answers that started it:

> "do we have some kind of ops dictionary so that ops get reused or retooled if similar?"  ->  no (below).
> "they should be grouped like 'banks' with weights."
> "inside of an ops dictionary / wiki that is also human readable at any given time by converting to html using script."

Related: `ROBOT-CHAT-BLUEPRINT.md` (section 3.1, banks), `&.widgits/concept-bank/` (the format this reuses), `PRISC-OPS-ARCHITECTURE.md`, `&.widgits/_shared-lib/harness/README.md` (harness cases are meant to become bank data too, owner 2026-10-07).

## 1. Today: no dictionary (checked 2026-10-07)

- **No house-wide ops index or catalog exists** (searched for ops-dictionary / op-catalog / op-registry file names and doc mentions; nothing).
- What exists is **per-project and partial**:
  - **45 `default_op.txt` files**, one per prisc project: `name type handler {description}`. Each registers only the custom ops *that project's pal calls*; it is a launch table, not a map of the house.
  - **`#.ref/menu/event_commands.registry.pdl`**: data-driven registry for **event commands only** (each wraps one op with substituted params).
- **711 op sources** under `*/ops/*.c`. Same names are copied around (the house rule is "duplicate rather than share a header"): `ledger_append` 8 copies / **4 distinct versions**, `json_parser` 8 / 2, `ledger_peers` 4 / 3, `move_player` 4 / 3. Nothing says which copy is newest, which are equal in purpose, or that a similar op already exists under another name, so reuse and retooling is by memory and `grep`.

## 2. Proposal: an ops bank in the Concept Bank format

Use the format the house already has (`&.widgits/concept-bank/data/{masters,spokes}`; schema in `masters/force.pdl`): a **master** is a named abstract concept, a **spoke** is a leaf that points at masters through weighted slots, **weights live only on the spoke** (the master's mirror table is derived, rebuilt by `concept_mirror_rebuild.sh`).

- **Spoke = one op** (by name + path), e.g. `ledger_append`.
- **Masters = what an op does / touches**, e.g. `append-only-log`, `kv-state-file`, `process-lifecycle`, `pdl-parse`, `render-frame`, `phone-message`, `scratch-test`.
- **Slot = `POINTS_TO=<master> | WEIGHT=<-1..1>`**: how strongly the op *is* that thing (0.9 = it is the append-only-log writer; 0.3 = it touches a log on the side).
- **"Similar" falls out of the shape**: two ops that point at the same masters with high weights are retool/merge candidates; a new need ("append a line to a ledger") is a query: find spokes with high weight on `append-only-log` before writing a new op. **Weights make the grouping graded, not a flat folder.**
- **Variants** (the 4 `ledger_append` versions) are one spoke with several `PATH` rows and a content hash each, so "same op, drifted copies" is visible and a consolidation is a measurable diff.
- The **harness cases** (`harness/cases/*.pdl`) are the same kind of data: a case is a spoke whose slots point at the masters it exercises (e.g. `play-mode-file`, `map-access`), and its PASS/FAIL ledger rows are evidence. This is the "harness as bank weights, usable as training" idea, with the same record type.

## 3. Build order (when the owner says go)

1. **Generator** (an op, run by a pal): scan `*/ops/*.c`, write one spoke file per op *name* with its paths + hashes + the first header comment as DESCRIBE text. No weights yet (honest: nothing learned).
2. **Seed masters** by hand (10-20 concepts) and weight the ~50 most-used ops by hand; the **DESCRIBE -> SCORE -> VALIDATE -> PROMOTE** loop in `ROBOT-CHAT-BLUEPRINT.md` / `AUTO-PROMOTION-RULE.md` can propose weights for the rest, **validated before promotion** (never auto-trust).
3. **Query op** (`ops_bank_query_op`): `similar <op>` and `find <master>` for humans and for the pal harnesses.
4. A **drift report**: ops with several differing copies, ranked by how many files include or exec them.

## 3b. The wiki: human-readable at any time, as HTML (owner, 2026-10-07)

The dictionary is **also a wiki**. Rule: **the bank data is the only source of truth; the HTML is derived and disposable** (same discipline as the masters' mirror tables: never hand-edit a generated copy).

- **Converter**: one op, `ops_wiki_html_op`, run by a small pal (`exec` the op, `halt`), regenerates the whole site on demand: `ops-wiki/index.html` plus one static page per op, per master and per drift report. The owner said "script"; given the standing direction (pal + ops, not sh), it is an op under a pal; the way to run it is one command either way.
- **Pages**: *op page* = name, one-line purpose (its header comment), usage line, every copy with path + hash + "newest / drifted", its masters with weights (links), the ops most similar to it (shared masters), the harness cases that exercise it, who includes or execs it. *Master page* = the ops that point at it ranked by weight. *Index* = all masters, then an A-Z of ops, plus the drift report (ops with several differing copies) and an "unclassified" list (spokes with no weights yet), so gaps are visible, not hidden.
- **"At any given time"**: regenerate at the end of every ops scan, and expose a "regenerate" action; each page header shows `generated <time>` and the bank revision (a count/hash of the spoke files), so a stale page announces itself. No server needed: plain files, opened from disk. It can later be published as a page for others (the Artifact tool) from the same output.
- **Human edits** go to the *source* (a description or a weight in the bank, via the validated promotion loop), never to the HTML; the page can carry a "propose a change" line that names the file to edit.
- Build order (extends section 3): step 3b after the query op, because the pages are just the query results rendered.

## 4. Before building (house rule: read first)

`feedback-check-docs-before-building-ai-track`: read `ROBOT-CHAT-BLUEPRINT.md` and the latest `2do.md` before touching Concept Bank code. This doc stays inside the existing bank format; it adds no new mechanism. Do **not** write into the real `concept-bank/data` until the owner approves the master list (a scratch bank folder first).

## 5. Open questions

1. A separate **ops bank instance** (own folder) or masters added to the shared house bank? (Recommend separate, so a bad weight cannot touch the chat personalities.)
2. Who approves promoted weights: the owner, or the headstone/official-LLM chain (as for quests)?
3. Is a drifted copy ever *intentional* (the project-local copy rule), or should the report flag every divergence?
