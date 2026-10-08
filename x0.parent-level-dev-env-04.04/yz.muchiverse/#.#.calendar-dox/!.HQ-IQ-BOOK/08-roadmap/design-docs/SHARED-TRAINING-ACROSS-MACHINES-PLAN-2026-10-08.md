# Training our own models, and sharing the training across machines (plan, 2026-10-08)

Status: **PLAN, nothing built.** Owner (2026-10-08): free models hit their limits; are we training our own; have we started tomom; "it can train on other machines as well. That's the point. They can share their training."

## 1. Where we really are (checked 2026-10-08)

- **No model weights are trained anywhere today.** `LEARNING-LOOP-BANKS-WEIGHTS-NO-REPROMPT-DESIGN.md` says so, and it says tomom has "no data yet". All learning so far is **evidence plus arithmetic**: delegation-bank FEEDBACK rows graded by Laplace `(reward+1)/(reward+punish+2)`, hand-scored word-bank rows, answer banks (`answers.tsv`, 443 rows), the hidden-layer association tables, entity grades (levels/MP). Joints are all at autonomy 0 and AUTO_PROMOTE is off.
- **tomom exists as dormant code, not a running trainer.** `#.Z.HUMAN_LLM/3.stage.llm.tomom…` is a hand-built mini language-model pipeline of 454 files in C: `attention.c`, `mlp_layer.c`, `backward_prop.c`, `trainer.c`, `vocab_model.c`, `chatbot_moe_v1.c`, `http_server.c`, with corpora and attention score files. The `tomom-hq` window (`&.hq-apps/tomom-hq`, with `tomom_manager.+x`) edits tomom weights/rows but there is no live training behind it. I have not built or run the C pipeline, so I cannot say it still compiles or trains.
- **Why free models hit limits:** the Groq free tier is about 200,000 tokens per model per day; both big models were used up in one session. Own models would not have that ceiling, but nothing trained exists to replace them yet.

## 2. What "share their training" should mean here

Three layers, in this order. Each is useful alone, and the first two need no GPU.

1. **Share evidence (rows), not weights.** Every learning record is already an append-only line with an id: FEEDBACK rows, SCORE rows, answer rows, observations. `palnet_peer` already replicates append-only lines between machines (outbox to inbox, exactly-once, address memory, harness-covered). So every machine can send its new rows and receive the others'. Because grades are a pure function of the rows, **every machine computes the same grade from the union**, with no coordination and no trusted server. This also makes the free-model budget go further: an answer a worker translated on one machine is in every machine's answer bank.
2. **Share compute for the data work.** Train/score tasks (batches of translation, scoring, tagging) are queued as quests (`^.grave`); any machine's worker can take one, run it on its own free quota or local model, and post the result. Debil, the Mac and this desk each have their own quotas and CPUs; the Mac runs the judge model. Splitting a batch across three quotas triples the daily ceiling.
3. **Share learned parameters.** Only after layers 1 and 2 produce data worth training on: train small models (tomom's attention/MoE code, or fine-tune a small local model) on each machine's rows, then combine. Start with the simplest combination that is auditable: **average weight files** from machines that trained on the same base and the same row set, or better, **retrain on the union of rows** so any machine can reproduce any model from the ledger. Federated averaging is a later option.

## 3. Rules that make sharing safe

- **Rows only; never user data.** A shared row must carry no wallet, key, phone history, chat text of a private room or path. Each row type gets an allow-list of fields; the sender filters, and the receiver rejects anything outside it. (The install doc's rule on user packs applies.)
- **Idempotent and ordered by id.** The peer drops replayed lines (done); consumers must also dedup by row id so a replay after a restart cannot double-count a reward.
- **Provenance.** Each row keeps `source` (which machine, which model or person) so a poisoned or low-quality contributor can be down-weighted or dropped, and so the 120b/20b attribution bug (206 rows booked to the wrong worker) cannot hide. Fix that bug before sharing.
- **Autonomy stays 0.** Receiving rows may change *grades* automatically (pure arithmetic); it must not promote an edit or change a joint without a human, exactly as today.
- **Trust tier.** On a hidden LAN, honor-based sharing is acceptable for now (owner's call); signed rows come with the signing gate.

## 4. Build order (each step a pal harness with a mutant)

1. Fix the worker attribution bug and add a `source` field to FEEDBACK rows. (W)
2. A **ledger replication op**: tails chosen ledgers into the peer outbox with the allow-list filter, and merges the inbox into the local ledger by id. Harness: three scratch peers, each appends rows, all three end with the same union; replay adds nothing; a row with a forbidden field is rejected. (M)
3. A **grade check** harness: the same union gives the same grades on every machine (determinism). (W)
4. Distribute **batch quests** (translation/scoring) across machines and merge their answer banks; measure rows per day versus one machine. (M)
5. **Decide the first thing to actually train:** a small model on the answer/feedback rows (the owner chooses the target), reusing tomom's C code only after a read-only audit shows it builds. (owner + M)
6. Parameter sharing (average or retrain-on-union) once step 5 produces a model.

## 5. Decisions for the owner

What to train first (a phrase chooser for Eden talk, a translation scorer, the hidden-layer weights); whether tomom's old C pipeline is the base or a fresh small model; which ledgers may ever leave a machine; whether sharing is LAN-only or later wider.
