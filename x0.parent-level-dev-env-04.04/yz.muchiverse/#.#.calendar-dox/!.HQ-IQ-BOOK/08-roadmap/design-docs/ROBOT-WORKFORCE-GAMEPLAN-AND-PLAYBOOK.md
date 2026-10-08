# 🤖🪦👻 The Robot Workforce — gameplan and playbook (2026-10-07)

> 🎯 **The ask (owner, 2026-10-07):** "make as many robots as we need to delegate all these tasks, through headstones / ghosts, and be
> training IRL / tomom the entire time. What is the gameplan / playbook? Explain in emoji-heavy html / md after you architect it all out,
> create a new night class, and say how this informs the roadmap's scale and delegation."

**Status:** 📐 architecture + playbook. Almost nothing here is built; section 13 says exactly what is real, what is partial, what is only designed.
**Companions (read, not repeated):** `GRAVEYARD-GHOSTS-DESIGN.md` 🪦, `HAI-ROBOTS-PHONES-SERVER-DESIGN.md` 📱🖥️, `DELEGATION-FLYWHEEL-HORN-GHOSTS-DESIGN.md` 🔁,
`TASKS-AS-EVENT-DATA-DESIGN.md` 📜, `HARNESS-DELEGATION-PIPELINE.md` 🧪, `GAME-CONDUCTOR-ENTITY-AND-EDEN-DESIGN.md` 🔘, NIGHT 30 / 34 / 35 / 36 🎓.
This document is the **one place that joins them** and turns them into an order of work.

---

## 🧭 0. The whole thing in 12 lines

1. 🪦 A **gravestone** (headstone) holds the *truth*: every quest, its spec, its locked test, its history. Failed quests stay on the stone. That is the graveyard.
2. 👻 A **ghost** is a worker identity: a tier, a brain, a history, a skill grade. Ghosts do not own files; they do quests.
3. 🫀 A **soul** is a ghost that **possesses** an entity (Asa, Ava, a chicken, a robot). The body can die; the soul keeps its history and can possess another.
4. 📱 Every entity has a **phone**. Ghosts talk only by phone. 🖥️ One **server** is the single writer of every inbox and keeps the ledger.
5. 🧪 A **harness** is the judge: a pal test whose bytes are hashed and locked *before* a worker starts. Models propose. Code and a person decide.
6. 🪜 The **router** picks the cheapest tier that can pass the harness: deterministic code → local student → free cloud worker → manager (Claude/Grok) → owner.
7. 🔁 Every attempt, pass or fail, is **appended** to history. Failures become **curriculum**; passes become **reusable harnesses**.
8. 🎓 Three **training loops** run all the time: harness loop (every quest), IRL loop (acts in the live house), tomom loop (corpus → student model).
9. ⚖️ Autonomy is **earned per skill** on the Joint contract (0 → 3), never granted globally, never auto-promoted.
10. 🧱 Fleet size is limited by **four real budgets**: free-quota, CPU, review attention, and merge safety. Not by how many robots we can imagine.
11. 🚫 **Free models only** (owner policy). No paid credits. Add free sources when we need more.
12. 🕸️ The same server/phone model gets a **visual network console** (owner idea, section 14): ports, links, groups, for virtual, LAN and p2p.

---

## 🗺️ 1. The map (who talks to whom)

```
                       👤 OWNER  (approves, steers, sees everything)
                          │  phone / board / dashboards
                          ▼
   ┌───────────────── 🧑‍💼 MANAGER (Claude/Grok): specs, harnesses, verification, "done" ─────────────────┐
   │                                                                                                     │
   │   🪦 GRAVESTONE  ◄────── truth ──────►  🖥️ SERVER (single writer, routes, ledger, limits)          │
   │   quests/ Qnnn/                              │  inbox/outbox per phone (append-only, marker growth)  │
   │   QUEST.md  harness/  LOCK.sha256            │                                                       │
   │   history  verdicts                          ▼                                                       │
   │                                  📱──👻 ghost A (tier W: free cloud)   ── worktree ghost/A/Qnnn      │
   │                                  📱──👻 ghost B (tier S: local student)── worktree ghost/B/Qnnn      │
   │                                  📱──👻 ghost C (tier D: deterministic) ── just runs ops             │
   │                                  📱──🫀 soul in 🧑‍🌾 Asa / 🐔 chicken (possession, mode off/assist/auto/train)│
   └──────────────────────────────────────────────────────────────────────────────────────────────────────┘
                 🧪 harness runner (prisc + ops) ── verdict lines ──► back to the stone
                 🏦 banks (behaviors / ops / phrases) + ⚖️ weights (joints)  ◄── learning lands here
                 🎓 tomom pipeline (corpus → student)   🌍 IRL: Eden, DSR, ring-board (real play outcomes)
```

Reuse rule (house standard, again): **there is no separate bot subsystem.** A robot is a pal; a ghost is a roster folder; a task is event data; a
message is a line in a phone file; a judge is a pal harness. Everything below is *use* of parts that exist.

---

## 🪜 2. Tiers and the router (cheapest sufficient brain)

| Tier | Brain | Examples | Cost | Can do | Cannot do |
|---|---|---|---|---|---|
| **D** 🧮 | deterministic code | compiled ops, harness cases, event pages | ~0 | anything fully specified | judgment |
| **S** 🎒 | local student model (Ollama, tomom corpus) | small scoped edits, classification, describe-not-classify | CPU only (weak machine: `nice -n 15`) | narrow, repeated tasks it was trained on | long reasoning |
| **W** 🔧 | free cloud worker via HORN | Groq gpt-oss-120b / 20b / qwen3.8-27b, OpenRouter `:free` | **free**, quota-limited | scoped implementation against a locked harness | architecture |
| **M** 🧑‍💼 | manager (Claude / Grok) | specs, harnesses, review, verification, merges | tokens (the scarce thing) | decide the shape | be everywhere |
| **O** 👤 | owner | direction, approvals, taste | attention | final say | — |

**Router rule (data, not code in the model):** a quest carries `tier_min`. The server offers it to the lowest tier whose **grade on that skill** is high
enough and whose **budget** allows. On failure it **escalates one tier**, appends why, and the failed attempt is kept as curriculum for the lower tier.
Grade = `(reward + 1) / (reward + punish + 2)` per ghost per skill (the Joint contract, spec §0). The router only reads it; it never rewrites it.

---

## 📦 3. The unit of work: the **Quest Packet**

A quest is a folder on the stone. The existing `QUEST.md` header stays; the packet adds the parts that make delegation *safe and measurable*.

```
^.grave/quests/Q0nn-<slug>/
  QUEST.md          status header, prompt, onboarding, log           (exists today)
  scope.txt         the ONLY paths a worker may write (glob rows)   📐 new
  harness/          the judge: <name>.pal + cases/<name>.pdl         (pattern exists)
  LOCK.sha256       sha256 of harness bytes, written BEFORE assignment 📐 new
  reference/        optional: a known-good solution kept SEALED from the worker 📐 new
  budget.pdl        max attempts, max wall seconds, max free-quota units, escalate_to 📐 new
  attempts/001/     each try: diff, run log, verdict line, worker id, tier, model, tokens (append-only) 📐 new
```

Rules that make it a packet and not a wish:
1. 🔒 **Harness first, lock second, worker third.** A worker that edits the harness is disqualified automatically (hash mismatch).
2. 🧪 **A harness must be shown able to fail** (a deliberately wrong reference) before it may judge anyone. (House rule, proven in q001/q002.)
3. 🧱 **Scope is a file, not a sentence.** The runner compares `git diff --name-only` with `scope.txt`; anything outside it fails the attempt.
4. 🌱 **Base is the tip at claim time.** The runner creates the worktree from the current tip of the target branch and **refuses a stale base**. (Lesson of this week: a worker started on a base 275 commits old and could not even see the code. 🪦 see section 12.)
5. 💸 **Budget is explicit**, including quota units. A quest with no budget is not assignable.
6. 📜 **Everything appends.** Attempts are never edited; the stone keeps failures.

---

## 🔄 4. Lifecycle (state machine)

```
 open ──judge──► pending ──approve──► claimed ──start──► active ──harness pass──► review ──manager verifies──► done
   ▲               │ deny (reason)        │ lease expires        │ fail / over-budget      │ rejects                ▲
   └───────────────┘◄─────────────────────┘◄─── escalate tier ───┴────────────────────────►└─► failed / abandoned ──► 🪦 stays forever + 🎓 curriculum
```

- `open → pending`: **assign** (push, server picks an available ghost) or **work** (pull, an idle ghost asks). Both go through the *same* gate.
- The **judge** (v1 rule-based: tier fits, ghost idle, quest unblocked, no conflicting lease) only *recommends*; the manager or owner approves. LLM judge later, same slot.
- `review → done` requires **the manager's own fresh build + fresh run + evidence** (house verification rule). A ghost's "it works" is a claim, not evidence.
- A lease has a timer. A silent ghost loses the lease; the quest returns to `open` with a log line. Two ghosts can never hold one quest (server = single writer).

---

## 🧑‍🤝‍🧑 5. The roster: roles we actually need

Specialize by **verb**, not by personality. Each role = a ghost template (`^.ghost/roster/_TEMPLATE`) with its tool permissions and a starter skill list.

| # | Role | Glyph | Tier | Tools it may use | First quest | Why it saves tokens |
|---|---|---|---|---|---|---|
| 1 | **Builder** | 🔨 | W | read, grep, edit/write inside `scope.txt`, run its own harness | `Q010` HORN error reporting fix | implements scoped pieces the manager already specified |
| 2 | **Tester** | 🧪 | D→S | open entity, drive by relay, read state files, write verdict | open one entity, report pass/fail | replaces manual "did it appear?" rounds (owner is tired of those) |
| 3 | **Reviewer** | 🔍 | W | read-only diff + harness output | review every Builder attempt before the manager | halves manager reading time |
| 4 | **Librarian** | 📚 | S | read, write only `docs/` indexes | keep roadmap + backlog + night index in sync | the index rots otherwise |
| 5 | **Scribe** | ✍️ | W | read, write docs in scope | turn commits into doc deltas | docs stay mirrored (house policy) |
| 6 | **Teacher** | 🎓 | W | read histories, write curricula | turn a failed attempt into a curriculum item | feeds Tier S |
| 7 | **Scout** | 🔭 | W | read-only across branches | summarize what other branches changed before a pull | exactly today's job |
| 8 | **Gardener** | 🌱 | S | possess a farm entity in Eden | tend plants/chickens by the day tick | IRL training in a safe sandbox |
| 9 | **Courier** | 📨 | D | `hq-ftp` / ssh / pack install | ship a pack to a LAN peer | no model needed |
| 10 | **Referee** | ⚖️ | D | run harnesses only | the harness runner itself | the judge is never a model |
| 11 | **Quartermaster** | 🧾 | D | read ledgers | quota, spend, CPU accounting; stop the fleet at limits | keeps "as many robots as we need" honest |
| 12 | **Archivist** | 🗄️ | D | backups, sha lists, user-data branches | the backup + verify steps before any bulk change | prevents the data-loss class of incident |

Tier D roles are **ops with a phone**, not models. A large share of "robots" should be deterministic, because a deterministic robot is free, fast and cannot hallucinate.

---

## 🎓 6. Training the whole time: three loops, one ledger

### 6.1 🧪 Harness loop (every quest)
Quest → attempt → verdict line. Gives **hard, deterministic** reward/punish per (ghost, skill). This is the only loop allowed to move a grade.

### 6.2 🌍 IRL loop (the live house is the gym)
Ghosts act on real entities: Eden (Asa/Ava farm, trade, talk), DSR economy, ring-board, Footrace Fu. The play *outcome* (balance of the ledger, day counts, survived/not)
is the signal. Possession modes on a soul: `off` (nothing), `assist` (suggests, human clicks), `auto` (acts), `train` (acts and every action is scored).
The Eden harnesses (`eden_loop` 305 checks) already give us a **simulator with a referee**, which is exactly what IRL training needs before touching the live board.

### 6.3 🗣️ Tomom loop (corpus → student)
Attempts, chats, phrases and verdicts append to banks; the `tomom` pipeline (dormant, real: `#.Z.HUMAN_LLM/3.stage.llm.tomom...`; binary absent today) trains **Tier S** on
what Tier W/M already solved. The house model (NIGHT 30): `Watch → DESCRIBE (fixed line) → validator → human review`. **Describe, not classify.** Promotion is a human act.

### 6.4 What is saved, and where (so learning is never "in a chat")
| Artifact | Meaning | Lives in | Status |
|---|---|---|---|
| `attempts/NNN/` | one try: diff + log + verdict + tier + tokens | the quest folder | 📐 |
| `history.txt` (ghost) | one line per event, append-only | `^.ghost/roster/<id>/` | 📐 (format designed) |
| `ledger.txt` (server) | every routed message | `^.hai-server/` | 📐 |
| learning harness | a failed→fixed pair frozen as a case file | `harness/cases/learned/` | 📐 |
| `hand_scored_exchanges.txt`, `irl_apply_signal.sh`, `curricula_gen.sh` | existing HORN learning hooks | `^.hai-horn/` | 🟡 exist, not wired to the board |
| weights ledger `W|…` | joint weights per skill | per Joint contract | 📐 spec §0 |

**Rule:** a lesson counts only when it exists as a *case file a harness can run*. "The model learned it" is not evidence.

---

## 🫀 7. Souls and bodies (how a ghost "goes to work" in the world)

`POSSESS | ghost_id | entity_id | since | by | mode` and `RELEASE` rows in an append-only ledger. Consequences, all already decided:
- A soul drives **its entity's tasks**; the entity's phone is the soul's mouth. Talking to the entity talks to the soul.
- The entity may die (Eden death switch defaults **off**); the soul keeps `history` and may possess another body.
- One soul per body at a time; the server grants the possession like a lease.
- **Kits and grants** (what items imply which skills: *seed → offers plant*) belong to the *conductor* 🔘, not the soul, so a soul can be swapped without re-granting.

---

## 🧱 8. Fleet governance: how many is "as many as we need"

Four real ceilings. The Quartermaster 🧾 enforces them; none is a model's opinion.

1. **🆓 Free quota.** OpenRouter free: **50 requests/day per account** (observed, HTTP 429 `free-models-per-day`). Groq works; its limits are **not measured yet** (measure before planning on them). More free sources = more capacity; each gets its own provider row.
2. **🖥️ CPU.** Weak machine. Heavy ghosts run `nice -n 15 ionice -c3`; at most *N* concurrent worker processes (start N=2); builds are serialized.
3. **👁️ Review attention.** Every attempt costs someone to read it. A Reviewer ghost 🔍 filters first; the manager reads only attempts that passed harness + review.
4. **🔀 Merge safety.** Ghosts commit only to `ghost/<id>/<quest>` in their **own worktree**; only the manager merges; never `main`, never another tool's branch, never user data.

**Spawn limits** (server config, already designed): max depth, max live children per robot, max total entities, per-entity spend cap, an owner kill switch.
**Backpressure:** when quota or CPU is spent, the queue *waits*; it never degrades to a paid model.

**A sizing formula (hypothesis, to be measured):**
`useful_attempts_per_day ≈ min( free_requests_per_day / avg_requests_per_attempt , cpu_slots × hours / avg_attempt_minutes , reviews_per_day )`.
With OpenRouter alone at 50 requests/day and ~5 requests per attempt, that is ~10 attempts/day from one account: **small**. This is why Tier D and Tier S robots matter most.

---

## 🛡️ 9. Safety rules (non-negotiable)

- 🔐 **No keys in git.** Keys live in git-ignored `raw_*.txt`. A ghost never prints them.
- 🚪 **Permission denial is final for the one who was denied.** A ghost (or any agent) that is blocked does **not** ask a peer to do it instead. That is *permission laundering*. It reports the block to the manager; the manager asks the owner. (Seen live this week: a worker was denied a `git merge --ff-only`, stopped correctly, and handed back.)
- 🧑‍⚖️ **HORN's approval gate for write/edit/exec stays ON** for ghosts.
- 🧮 **Models describe and propose; code and a person decide.** No auto-promotion of weights, skills or kits.
- 🗄️ **No bulk change of user data without a tarball + sha list outside the repo, verified.** (Archivist 🗄️.)
- 🌿 **Own branch, own worktree, from the current tip.** No `git stash`, no `reset`, no `add -A` in a shared index.
- 💤 **Never `pkill -f`** from a tool shell; stop processes by recorded pid.
- ✅ **"Done" = fresh build + fresh run + real evidence** (diff, state file, verdict). A clean compile is not evidence.
- 🧯 **Kill switch**: owner can stop the whole fleet from the server window.

---

## 🛤️ 10. The gameplan: phases with exit tests

Each phase ends with a **test that can fail**. No phase starts before the previous one's test passes.

| Phase | Build | Exit test (pal harness) |
|---|---|---|
| **P0** 🩺 *fix the sensors* | `Q010`: HORN op logs HTTP error bodies and **stops on 429**; `Q011`: quest packet format (`scope.txt`, `LOCK.sha256`, `budget.pdl`) + a `quest_check` op | forged harness bytes are rejected; a 429 stops the run and says why |
| **P1** 🔨 *first runner* | one **Builder** ghost: claim a quest → fresh worktree from tip → HORN turn(s) → attempt folder → harness → verdict line | q001-style quest passes end to end with no manager typing; a bad worker fails and escalates |
| **P2** 🪦 *the stone shows* | board window live (Q006 layout), phones route (`Q009` `phone.send`/`server.route`) | owner posts a quest from the board; the ledger shows the routed `task` |
| **P3** ⚖️ *pending + judge* | `pending` status, rule-based judge, approve/deny | two ghosts racing for one quest: exactly one lease |
| **P4** 🧑‍🤝‍🧑 *specialize* | Tester 🧪, Reviewer 🔍, Scout 🔭, Quartermaster 🧾 | Tester opens one named entity and reports pass/fail on the stone; Quartermaster halts the fleet at a set quota |
| **P5** 🎒 *students* | Tier S: curricula from failed attempts; tomom binary built; local model on the LAN | a student passes a quest class it failed last week, measured by the same locked harness |
| **P6** 🌍 *IRL* | Gardener 🌱 soul possesses Asa in `train` mode on the **Eden simulator** | scored day-N survival improves vs. a random-pick baseline across seeds |
| **P7** 🕸️ *network console* | section 14 | the console shows the server's real routes; a port move changes who may message whom |

Order logic: P0–P1 make delegation **safe and countable**; P2–P3 make it **visible and governed**; P4–P5 make it **cheap**; P6 makes it **learn from the world**.

---

## 📏 11. Metrics (counted, not asserted)

- 🎯 **First-pass rate** (harness green on attempt 1) per tier and per skill.
- 🪜 **Escalation rate** (how often W→M).
- 💸 **Cost per `done` quest** in manager tokens (the thing we are trying to save) and in free-quota units.
- 🔁 **Harness reuse** (times a locked harness judged >1 attempt or later regression).
- 🧯 **Regression rate** (a `done` quest broken within N days).
- ⏱️ **Time to verdict.**
- 🧠 **Student lift** (Tier S pass rate before/after a curriculum).
- Break-even rule of thumb (hypothesis): delegation pays when `tokens(spec + harness + review) < tokens(manager doing it)`; cheap to check per quest.

---

## 🪦 12. The failure museum (real incidents this week, kept on the stone)

| 🪦 | What happened | Rule it produced |
|---|---|---|
| 1 | a worker's worktree base was **275 commits old**; the code it was told to extend did not exist there | base = current tip at claim time; refuse stale |
| 2 | HORN "failed" for hours; the real cause was HTTP 429 (daily free limit) hidden by the op | surface HTTP errors; stop on 429; never guess a timeout |
| 3 | a harness runner **trims argument whitespace**; a "space" case was invalid | a harness must be shown able to fail *and* be shown valid |
| 4 | an output flood from printing every FAIL line | cap output; the verdict line is the interface |
| 5 | a restart script killed the Mac desktop, then **failed to rebuild** (non-login ssh PATH lacked `pkg-config`) and left it down | any restart tool must relaunch or report loudly, and run in a login shell (a reusable harness is on the list) |
| 6 | a generated entity had a **placeholder** `Cli-io` row that did nothing | generators must not emit fake affordances; guard with an `EXPECT_LACKS` case |
| 7 | a worker was blocked by the permission layer and did **not** route around it | the correct behavior; make it the rule |
| 8 | screenshots and `strace` are blocked here, so a visual change could not be verified | state files and logs are the evidence channel; design for it |

---

## ✅ 13. Honest status (what exists, today)

| Piece | State |
|---|---|
| 🧪 pal harness runner, 16 live suites green (2026-10-07) | ✅ real |
| 🔧 HORN with tool calling, provider ladder, **Groq working** | ✅ real (🟡 hides HTTP errors) |
| 🔁 delegation pilots q001 (clamp) and q002 (weighted pick) | ✅ passed with locked harnesses |
| 🪦 board folder, 9 quests on the stone | ✅ real, no window |
| 📱 phones in inventories | 🟡 55 phones + uids done (Q005) |
| 🖥️ server router | 📐 design + `router.pal` + `tunables.conf`; `Q009` open |
| 👻 ghost roster | 📐 template only; **zero ghosts appointed** |
| 🔨 ghost runner (claim→worktree→HORN→harness) | 📐 not built |
| 🔒 `LOCK.sha256`, `scope.txt`, `budget.pdl`, `attempts/` | 📐 new in this doc |
| ⚖️ pending/judge/approve | 📐 designed |
| 🫀 possession ledger | 📐 designed (Eden conductor has kits/grants ✅) |
| 🎒 tomom Tier S | 🟡 pipeline real but dormant, binary absent |
| 🌍 Eden simulator (referee) | ✅ real, scratch-verified; GUI play unverified |
| 🧾 Quartermaster, quota accounting | 📐 not built; Groq limits unmeasured |
| 🕸️ network console | 📐 idea only |

---

## 🕸️ 14. Owner idea (2026-10-07): the house's own server / switch / router GUI

> "we can make our own server/switch/router's GUI in network for virtual / LAN / p2p management, visual layers, port assigning, etc."

**Fit:** the 🖥️ server already *is* a message router with permissions and rate limits (`server.pdl`, `tunables.conf`, `ledger.txt`). The console is its **window**, and the same
window can also show the real network the house touches.

**Three layers on one canvas** (like layer toggles in a network tool):
1. 🫧 **Virtual** — entity phones as *ports*; ghosts, robots, souls as *endpoints*; a grave/project as a *VLAN-like group*. A link = "may message".
2. 🏠 **LAN** — hosts from the internal list (`LAN-HOSTS.md`: the Mac, debil, the Pi-class boards), their ssh reachability, `hq-ftp` transfers.
3. 🌐 **p2p** — `palnet_peer` links (today hard-coded to 127.0.0.1; a cross-machine plan exists).

**Port assignment = permission data.** Dragging a port into a group edits `server.pdl` rows (who may message whom, rate, size). That is the *same* data the router already reads, so the GUI is
a **view and editor over config**, not a new protocol. Every edit is appended to a change ledger.

**Honest scope:** this models *our* message routing and read-only facts about real hosts. It does **not** do real Ethernet layer-2 (VLANs, STP) — that needs OS/root and hardware we do not
control. Any "real" layer-2 view would be read-only (`ip link`, ARP) at first. **Build order:** read-only topology from the ledger and the LAN list → port/group editing on `server.pdl` →
live traffic bars from the ledger → p2p and `hq-ftp` panels. It is **P7**, after the server routes anything at all.

---

## 📈 15. How this changes the roadmap's scale and delegation

- 📉 **Scale unit changes** from "features the manager builds" to "**quests the fleet clears per day**", bounded by section 8's four budgets. Roadmap areas now carry a *delegability* tag:
  🧮 D (deterministic, build now by anyone) · 🔧 W (scoped, harness-able) · 🧑‍💼 M (shape-deciding, manager only).
- 🧱 **Every roadmap area gets a "first quest" and a locked harness** before it is scheduled. No harness, no delegation.
- 🗺️ **New areas** added to the technical spec (§23 Robot workforce, §24 Network console) and to the state-of-play index.
- 🔁 **Backlog items 32–36** record: this workforce, the quest packet, the failure museum, the network console, the restart/ops harness.
- 🧑‍💼 The manager's job becomes: **specs, harnesses, verification, merges**. Implementation migrates down the tiers as grades allow.

---

## ❓ 16. Open questions for the owner

1. 🧾 **Worker concurrency start:** N=2 concurrent worker processes OK on this machine?
2. 🪦 **Naming:** keep `^.grave` / `^.ghost` / headstone, or a final name now that the board will be a product surface?
3. 👻 **Glyph per tier** (👻 worker, 🫥 student, ⚙️ deterministic) or one ghost glyph?
4. 🌿 **A separate worktree per ghost automatically** (recommended) or shared?
5. 🧑‍⚖️ **Who approves gated actions** during P1–P3: manager only, or manager + owner-visible log?
6. 🆓 **More free sources** to add first (Groq limits measured, Ollama on the LAN host, another OpenRouter account is *not* a plan — policy is per account)?
7. 🕸️ **Console scope:** virtual-only first, or LAN view in the first cut?

---

## 🧰 Appendix A — first quests to post (ids continue from Q009)

| id | quest | tier | size | exit test |
|---|---|---|---|---|
| Q010 | HORN op: log HTTP error body, stop on 429 | W | S | harness feeds a stub 429; run stops with the reason |
| Q011 | quest packet: `scope.txt`, `LOCK.sha256`, `budget.pdl` + `quest_check` op | W/M | M | forged harness rejected; out-of-scope diff rejected |
| Q012 | ghost runner v0 (claim → fresh worktree from tip → HORN → attempt folder → harness → verdict) | M→W | L | q001-class quest end to end |
| Q013 | Quartermaster op: read ledgers, stop the fleet at quota/CPU limits | D | S | stops at a set limit in a scratch run |
| Q014 | measure Groq limits (documented, not assumed) | D | S | table of observed limits |
| Q015 | Tester ghost v0: open one entity via relay, report pass/fail | D→S | M | passes on a good entity, fails on a broken fixture |
| Q016 | restart-desktop ops harness (login shell, relaunch-or-loud-fail) | D | M | stub build fails → harness says so and relaunches the old binary |
| Q017 | `hq-ftp` transfer app (spec exists) | W/M | L | pack arrives with matching sha on a second host |
| Q018 | store: install op `git clone` + unpack against a local test repo | W | M | idempotent install, rollback on failure |
