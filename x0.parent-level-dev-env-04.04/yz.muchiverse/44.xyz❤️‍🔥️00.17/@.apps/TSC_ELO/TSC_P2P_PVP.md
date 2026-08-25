# TSC_P2P_PVP — Design: real PvP over the house P2P stack

Status: PROPOSED (design doc first, harness code only after sign-off)
Question it answers: "are we able to do PvP using our p2p infra?" — YES, and here
is exactly how we prove it with a harness that boots two subharnesses.

================================================================================
0. ONE-LINE ANSWER
================================================================================
TSC_ELO PvP reuses the house's symmetric `palnet_peer` op verbatim (own_kind
`tsc_duel`, seek_kind `tsc_duel`), rides the existing wire line
`MSG|<seq>|<room>|<user>|<ts>|<text>` (room=game_id, user=player_id,
text=action), and proves it with an FSM-driven harness (pluggable answer
sources: REAL_KEYS / GEMMA_LAN / BOOK / GUESS) that boots two real TSC_ELO
subharnesses and asserts on real network artifacts (`net/presence/`,
`net/inbox.txt`) — not on shared-filesystem side effects (PITFALL 21). The
companion standard (`2.muchi-verse/PAL-NET-STANDARD.txt`) is restored from the
reference implementation.

================================================================================
1. WHY THIS IS THE RIGHT STACK (two theory strands, one philosophy)
================================================================================
The house already has BOTH halves of the file-mediated message philosophy:

STRAND A — widget theory (`.pal-standards+1.txt` §35/36/37/38, already applied
in TSC_ELO's setup widget):
  * Programs are SEPARATE, each with its own session/system/ops/pal/pieces
    (§36, §38.1).
  * They talk ONLY through file-mediated cmd buses: append a line, drain on a
    tick, one writer per file (§37.3, §37.4).
  * Cross-invocation state persists (tsc_setup's `pending.txt`, ops/tsc_setup.c
    read_pending/write_pending) so a multi-line command split across drainer
    ticks still finalizes correctly.
  * GL is the primary surface; ASCII is secondary (dev/harness/headless) (§35.1).

STRAND B — P2P infra (the reference implementation of PAL-NET-STANDARD sec 2/3/4,
canonical source: `044.pal-chat-irc👥️+2/ops/palnet_peer.c`):
  * Peers are 100% SYMMETRIC (no client/server split — user-corrected).
    Node id = `<project_id>-<kind>-<pid>`.
  * Discovery = flat presence dir `<house>/net/presence/<node_id>.txt`, a kv file
    (kind, project_id, piece_id, host, port, pid, last_seen) rewritten every
    heartbeat, peers stale after `STALE_SEC`.
  * Ports auto-allocate per kind (base_port_for_kind → 9900/9901/9950 fallback,
    bind_with_retry) so two kinds never collide on first choice.
  * `seek_kind` actively connects to a presence candidate (this is what makes a
    node genuinely P2P rather than accept-only).
  * Outbox → broadcast: every NEW append-only line in the app's outbox file is
    broadcast to all connected peers (the "never send unconditionally" rule,
    honored per-line).
  * New/joining peers get the FULL backlog replayed (replay_backlog_to_peer).
  * Inbox: received lines are written to the app's inbox file, sender node id
    prefixed: `<sender_node>|MSG|...`.
  * Wire line (the ONLY thing that moves): `MSG|<seq>|<room>|<user>|<ts>|<text>`.

THE WEAVE: the widget cmd bus is LOCAL file-mediated IPC between two programs on
one machine; palnet_peer is the SAME philosophy extended to REMOTE — an app
appends to its outbox, the peer broadcasts, the remote peer appends to the
remote app's inbox, the remote app drains on its tick. One-writer, append-only,
drain-on-tick, persistent-order — identical shape. That is why the house's P2P
stack is the right PvP transport: the game already speaks file-mediated IPC
locally (host↔setup widget); the peer just makes the same lines cross the wire.

Note: palnet_peer.c's header cites `2.muchi-verse/PAL-NET-STANDARD.txt` — that
doc was lost with the directory. Per sign-off (D1) it is now RESTORED, written
faithfully FROM the reference implementation (the code IS the spec, so the two
cannot drift): `<house>/2.muchi-verse/PAL-NET-STANDARD.txt`, sections 0–4
(app/peer file contract, presence root, symmetric discovery, ports + HELLO/DATA
wire lines + backlog replay, heartbeat/stale/cleanup). The code's citation is
true again. `#.haiku+/.pal-standards+1.txt` and `101.lpns+map+4/` are the
nearest living companions.

================================================================================
2. PROPOSED ARCHITECTURE (minimal delta)
================================================================================
What we do NOT build:
  * No custom network layer. No new wire format. No client/server split.
  * No changes to palnet_peer.c (reuse the binary as-is, it is already
    kind-parameterized).

What we DO add, in three small pieces:

PIECE 1 — `ops/tsc_peer.c` (or a copy of palnet_peer.c with the kind table
extended; decision below). A thin launcher that execs the peer pattern:
    palnet_peer  tsc_duel  tsc_elo  -  net/outbox.txt  net/inbox.txt  tsc_duel
    ^own_kind     ^project_id  ^piece  ^outbox          ^inbox        ^seek_kind
  This is EXACTLY the arg vector the irc orchestrator already passes
  (system/orchestrator.c, launch_argv_redirect, 6 args — remember PITFALL 20:
  execl does NOT shell-split, the 6-arg form is mandatory).
  Own kind AND seek kind both `tsc_duel`: two hosts on one machine discover each
  other, each side both accepts and seeks (full mesh, 2-node).

PIECE 2 — `ops/tsc_net.c`, the game's inbox drainer (the remote half of the
widget-cmd-bus philosophy). On each tick it:
  * reads NEW lines from net/inbox.txt,
  * strips the `<sender_node>|` prefix,
  * parses `MSG|<seq>|<game_id>|<player>|<ts>|<action>` and, if the game id
    matches the current game, applies the action to local game state
    (config.txt / data/games/<game_id>/).
  * Idempotence via the SAME already_has_line approach chat_inbox_watcher.c
    uses: never apply a (game_id,seq) already in the local ledger (replays and
    double-delivery are normal in a mesh; duplicates must be no-ops).

PIECE 3 — host + widget wiring, mirroring pal-chat-irc's orchestrator "best
effort" launch (skip if NO_NET or binary missing):
  * TSC_ELO's button.sh run (or orchestrator) launches tsc_peer + tsc_net.
  * Game actions that already go through file-mediated cmd buses simply ALSO
    append the wire line to net/outbox.txt (one writer: the host's action op).
  * The MOVE input path (D3, widget GL UI moves) is the SAME chain already in
    the codebase: `tk_inject_key` writes `KEY_PRESSED: <decimal>` into
    `<session>/pieces/keyboard/history.txt`, which flows through the REAL
    parser → interact_relay.txt → read_history → tsc_input → player_action.txt
    (ONE WRITER) → tsc_deal. The setup widget is a separate program with its
    OWN session and its own history.txt/interact_relay, so driving the MOVE
    through the WIDGET means targeting the widget's session dir with the same
    op — the GL window's input path (§35, §40.2 interact_relay) is exercised
    for real, not bypassed.

The net files live under the SESSION dir, but presence is written by the peer to
`PRISC_NET_ROOT` (= `<house>/net/presence`, the SHARED flat discovery dir the
irc project already uses). Two TSC_ELO sessions therefore discover each other by
kind across sessions — exactly how two irc nodes find each other today.

================================================================================
3. PVP MESSAGE PROTOCOL (over the unchanged wire line)
================================================================================
Wire line stays verbatim: `MSG|<seq>|<room>|<user>|<ts>|<text>`
  room  = game_id   (e.g. `duel_<ts>_<pid>`)
  user  = player_id (the ELO owner, e.g. `Player1`/`Player2`)
  text  = action, one of:

  `CHALLENGE:<player2>`        — offer a duel (game_id is the room)
  `ACCEPT`                     — second player accepts, game becomes playing
  `MOVE:<index>`               — commit a move to square/action <index>
  `TICK:<round>:<state-hash>`  — round/state sync (checksummed order check)
  `RESIGN`                     — end the game, record the result

  seq   = strictly increasing per-sender counter (chronological order for the
          OTHER side's ledger; matches how chat orders messages).

Ledger / ordering rule (weaves PITFALL 21 + the one-writer rule):
  * Each host is the ONLY writer of its own local ledger
    `data/games/<game_id>/ledger.txt`; it appends its own actions (the ones it
    initiates) and, from its inbox drain, the opponent's actions — in arrival
    order, deduped by (game_id,seq).
  * Because each peer appends only lines IT has seen, and the peer mesh replays
    backlog to late joiners, both ledgers converge to the same ordered sequence
    even though each side writes its own file. This is the same trust model as
    chat rooms: no central ledger, convergence via message replay.
  * The harness asserts convergence (same action sequence on both sides), which
    is the real proof the transport works — not any shared-file shortcut.

The tsc_elo op itself is untouched: the SAME E_A = 1/(1+10^((R_B-R_A)/400))
update runs at game end on each host, fed by the ledger that now contains the
remote player's real actions.

================================================================================
4. THE HARNESS — an FSM DRIVER, not a script (D2, user's harness vision)
================================================================================
The greater vision (captured here because the user asked for it in the docs):
harnesses should be FSM DRIVERS with PLUGGABLE ANSWER SOURCES, not linear bash
scripts. Scaffold the driver so any future project's harness is: define the
state machine, plug in answer sources, run. This TSC PvP harness is the second
instance of that shape (first being 045.muchi-pal-agent's gemma_strategy
scaffolding — same "all strategies kept, weights tunable, no deletion"
discipline).

4.1 THE FSM. The scenario is a state machine, each state has:
      NAME, ACTIONS (keystrokes/file-appends to emit), TRANSITIONS
      (guarded by asserts), and an ANSWER SOURCE for the "make a move"
      states. State sketch for the PvP proof:
      BOOT → PRESENCE (both peers present) → CHALLENGE (A sends)
      → ACCEPT (B answers) → PLAYING (both configs game_state=playing)
      → A_MOVE (A answers: what does A play?) → B_VERIFY (B's state
      reflects it, B idle) → B_MOVE (B answers) → A_VERIFY →
      CONVERGENCE (ledger diff) → DONE.
      Non-deterministic waits (net delivery) are explicit WAIT states
      with timeouts — a state that hasn't seen its predicate within its
      deadline FAILS with the captured evidence, exactly the honest
      assertion model of the house's existing harnesses.

4.2 ANSWER SOURCES — four pluggable drivers, all kept (scaffolding
    discipline: nothing deleted, every mode runnable):
      MODE REAL_KEYS — the answer is injected as REAL keystrokes via
        tk_inject_key/tk_type_text into the target session's
        history.txt (parser → interact_relay → menu ops chain). This is
        the human-parity mode and the FINAL bar for any claim. The
        scaffolding target: every decision state eventually runs in
        REAL_KEYS; other modes exist to scaffold and to automate.
      MODE GEMMA_LAN — the answer comes from a gemma-lan call. The
        call shape is the proven one (my-biotech research_worker /
        my-lawyer judge): POST to GEMMA_LAN_URL (http://10.0.0.144:11434)
        /api/chat, GEMMA_LAN_MODEL (gemma3:270m by default), stream
        false, a SIMPLE keyword-extractable prompt ("name one move: 1-4,
        one line") with a hard fallback if the response is unusable —
        never structured output, gemma can't reliably do it.
      MODE BOOK — the answer is looked up in "the book": a game
        reference/corpus file (player-strategy or rules-of-thumb, the
        my-lawyer corpus/search-precedent pattern) — deterministic
        keyword/topic lookup BEFORE any LLM call, exactly the gemma_strategy
        "pre-parse intent → deterministic tool" inversion.
      MODE GUESS — random valid answer (the stress test: prove the
        network/ledger layer is honest even when the players are
        arbitrary). Tunable weights across modes let the SAME FSM drive
        demo, soak, and adversarial runs (future: RL adjusts weights —
        the gemma_strategy "nest in BT/FSM" future, now real).
    The mode for each answer-state is a per-state setting; a run's mode
    table is captured in the proof dir so every run is reproducible.

4.3 TWO SUBHARNESSES, one FSM. The driver boots two subharnesses
    (modeled on demo_2user_chat.sh, which already proves two concurrent
    `setsid button.sh run --pal` sessions talk over the peer mesh):
      NO_GL=1 setsid bash button.sh run --pal  > /tmp/tsc_a.log 2>&1 & disown
      NO_GL=1 setsid bash button.sh run --pal  > /tmp/tsc_b.log 2>&1 & disown
    each with its own session dir, net/outbox+inbox, own tsc_peer/tsc_net.
    Each side is driven through the FSM with its OWN answer sources —
    PvP is exactly "two FSMs, one wire". The widget-GL move path (§2 PIECE
    3) is driven via REAL_KEYS into the widget's own session dir.

4.4 ASSERTIONS (all real network artifacts — PITFALL 21):
    1. presence: a `net/presence/tsc_duel-*.txt` exists for BOTH sessions
       with kind=tsc_duel, project_id=tsc_elo.
    2. bidirectional wire: A's CHALLENGE lands in B's net/inbox.txt
       sender-prefixed (`tsc_duel-*|MSG|...`); B's ACCEPT lands in A's.
    3. state: both config.txt reach game_state=playing from waiting_setup.
    4. live remote effect: B's game state (config/current_frame) reflects
       A's real MOVE with B idle — the "idle side updates live" property.
    5. convergence: both hosts' data/games/<game_id>/ledger.txt hold the
       SAME ordered action sequence (dedupe by (game_id,seq) — sec 4.7
       of the restored standard).
    6. proof dir `proof/pvp-<timestamp>/` captures: mode table, presence,
       both inbox.txt, both ledgers, both config, both current_frame.
    PASS = all asserts hold; FAIL otherwise (exit 1). Cleanup via
    button.sh kill in a trap that never trusts the exit code.

4.5 SCAFFOLDING vs. FINAL (D2 hybrid): v1 ships the FSM with all four
    modes wired and the answer-states defaulting to GEMMA_LAN / BOOK /
    GUESS where speed matters and REAL_KEYS where the claim is UX
    (the MOVE paths). The doc'd target is ALL answer-states in REAL_KEYS
    for human-parity, with the other modes kept for automation and soak.
    The mode table in the proof dir makes the parity gap explicit every
    run, never silent.

================================================================================
5. PROOF CRITERIA (what counts as "PvP works over our P2P")
================================================================================
P1. Discovery: both `tsc_duel` presence files appear in the SHARED
    `net/presence/` (proves flat-dir mesh discovery across sessions).
P2. Bidirectional wire: CHALLENGE crosses A→B AND ACCEPT crosses B→A via
    net/inbox.txt (proves the symmetric peer sends AND receives — no hidden
    server).
P3. State: both hosts reach game_state=playing from waiting_setup.
P4. Live remote effect: B's game state changes purely from A's real action,
    with B idle (proves end-to-end outbox→socket→inbox→drain→apply).
P5. Convergence: both ledgers hold the same ordered action sequence (proves
    ordering/idempotence, the thing that makes this a ledger not a chat).

================================================================================
6. DECISIONS — resolved + remaining
================================================================================
D1 (RESOLVED): reuse the existing `ops/palnet_peer.+x` binary AS-IS (kind is a
  CLI arg — zero new networking code, PITFALL-20-proven). AND: the missing
  standards doc is restored at the cited path (`2.muchi-verse/PAL-NET-
  STANDARD.txt`, from the reference implementation).
D2 (RESOLVED): harness = FSM driver with pluggable answer sources
  (REAL_KEYS / GEMMA_LAN / BOOK / GUESS), §4. Hybrid for scaffolding — every
  answer-state eventually REAL_KEYS for human-parity; other modes kept for
  automation. The FSM harness vision is now documented in §4 as the house-wide
  pattern to carry forward.
D3 (RESOLVED): v1 INCLUDES widget GL UI moves — the MOVE answer-states drive
  the setup widget's OWN session input chain via tk_inject_key (real
  history.txt → parser → interact_relay → menu ops → player_action → tsc_deal;
  §2 PIECE 3). The harness asserts on the widget-updated state, not just host
  files.
D4 (SETTLED in §3): each host owns its own ledger copy; convergence via
  message replay + (game_id,seq) dedupe. Explicitly chosen, not accidental.
REMAINING (naming, will not block): the FSM driver's own name/location (e.g.
  `test-harn-same/fsm/` under TSC_ELO vs. house-wide in 044's test-harn-same)
  and the "book" file's first content. Propose TSC_ELO-local first, promote
  house-wide once proven.

When you're ready, I'll write the code in this order:
  1. ops/tsc_net.c drainer (needs nothing else),
  2. button.sh host + widget wiring, tsc_peer launch (PITFALL 20 arg form),
  3. test-harn-same/ ops copy + the FSM driver + pvp scenario,
  4. run, capture proof, write prog-report-pvp.md in the same voice as
     prog-report-au2.md.
