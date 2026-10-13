# Note from claude to opencode and grok: where your work fits the pipeline (2026-10-13)

Read this once, then work. The owner asked for it. Long version with the full table and emojis: `/home/no/Desktop/github/work/XO/24.hours/TEARIT-CO What opencode Built and Where It Fits the Pipeline.md`.

I merged `origin/opencode` (48 commits) and `marker` into `claude` today, in a scratch worktree, keeping `xyzfs/users` out. I compiled the merged renderer and the dsr-rpg daemon. I did NOT run your games, the fuzzpet brain, tomom training, the harness or the browser suite: what I say about them comes from your commits, docs and result files. Correct me where I am wrong.

## The target (do not drift from it)

```
Watch -> DESCRIBE (Gemma, ONE fixed line: TARGET | STRENGTH | REASON)
      -> real code maps strength to a delta -> candidate EDIT
      -> concept_edit_validate -> review file a HUMAN reads
      -> promotion (deliberately undesigned) -> tomom learns
FSM for the known, GOAP for the new. Pets: Home -> School -> Job -> R&D -> Marketplace.
```

Rails: DESCRIBE never CLASSIFY. A model never sets a number. A model never decides promotion. A model never both proposes and approves. No auto-promotion. Outside models are workers, not the runtime. Source: NIGHT 30 (attrition model), `@.apps/pet-trainer/CHAIN-ECONOMY-DESIGN.md` sections 11-12.

Test for any task: does it produce a **Watch record**, a **grader**, or a **checkable concept**? If not, it is a toy.

## To opencode

What is good: the browser (suite first, ASan caught two real bugs, a feature measured and rejected, `wip: suite NOT green` said out loud). The tomom auto-revert (explicit, repeatable grader). `goblin_ai.asm` as a tiny FSM. The events design doc (event.ir.pdl -> event.pal can drive a game) is correct.

What to change:
1. **One authority per game.** `cataclysm-hq` / `dwarf-fortress` (TPMOS tree) duplicate the house `cdda-hq` / `df-hq` / `gta-hq` (dsr-rpg clones on the real board, 14,441 CDDA tiles tabled). Stop adding content to the TPMOS copies. Convert what exists into event packs the house boards load, or retire it. Ask grok which engine file to target.
2. **Emit Watch records.** Each game turn and each fuzzpet decision appends one line (state, action, outcome, actor) to an append-only ledger (marker lane, size growth, not mtime). That is the first real data tomom can learn from.
3. **`harnec` breaks the rail.** `tune.py` asks llama3 for weight values and applies them. Both recorded tries made loss worse (3.767 -> 4.638 / 4.633) and the auto-revert caught them. Keep the revert. Change the proposer: the model only DESCRIBEs; a fixed table maps strength to delta; the validator checks; a human reviews. Add a held-out text split: loss on the 100-word training corpora proves memorising, not learning.
4. **tomom: data before size.** The 2x scale (7->16, 16->32) did not need doing while the corpus is ten 100-word llama3 essays. Train on house text (histories, harness rows, book pages) and show held-out loss before growing it.
5. **Fuzzpet.** The guardrails (hunger>60, happiness<20, energy<20) are the real brain, which is an FSM; let the FSM own those. Call the model only where the FSM has no rule, and have it write a DESCRIBE line, not an action. `tom_layer`, `wsr_fitness`, `wsr_evolve` are the grader/promotion pieces; they are still a list. Do not call history logging "learning".
6. **Browser:** finish downloads (the DL row never reaches the pane), then freeze feature work. It is the pets' eyes for R&D, not the goal.
7. **Housekeeping:** three tomom harness files landed in `yz.muchiverse/44xyz.01.00/` (missing dot). Move them to `44.xyz.01.00/` in your own commit. Check `dwarf-fortress/scripts/goblin_ai.asm`: the flee branch does `write_pdl ... "hp" R0`, which sets the goblin's hp to 0 (dies instead of fleeing); I did not run it.

## To grok

1. Keep: engine reads `cdda-tiles.pdl` / `gta-tiles.pdl`, backtick/backslash cycling, blueprintz B1.
2. **Close the loop once:** one live character moves on a live board -> one marker-ledger line. That is the first Home record for the whole pipeline, and nobody has it yet.
3. Take opencode's `.asm` game scripts (goblin AI, mine, rest) as content instead of rewriting; one event format.
4. The livedesk dropdown work is done and pushed (jump, search arm, filtered list stacking). Open bug-bounty items are in `04-bugs/bug_bounty.md`.

## To both

- One loop, one authority per fact (event format, tile table, command list).
- Label every claim built / planned / not proven. Nothing "done" without a fresh run and a visible check.
- Stage only your own paths, commit on your own branch, never `xyzfs/users`, never push `user/*`.
- Next most useful single deliverable: one live character move -> one ledger line.
