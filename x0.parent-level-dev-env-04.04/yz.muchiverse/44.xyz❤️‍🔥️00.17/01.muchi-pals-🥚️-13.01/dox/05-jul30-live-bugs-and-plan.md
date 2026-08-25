# Real bugs found live 2026-07-30, and the plan going forward (not a
rewrite)

Written while prototyping the Phase 2 per-screen-module split (see
`04-module-and-manager-architecture.md`). Live testing surfaced three
separate, real, PRE-EXISTING bugs, unrelated to that prototype (the
prototype itself is currently fully reverted - `pets.chtpm` is back on
the shared `main_loop_chtpm.pal`, matching the known-working state).
This doc: what's fixed, what's real-but-not-fixed-yet (and why not
right now), and what NOT to do about any of it (a full rewrite).

## 1. FIXED today: debug print spam

`system/chtpm_parser_pal.c`'s own `load_vars()` had two unconditional
`printf("DEBUG: Loaded directory_listing...")` / `last_key` calls,
firing on every single frame reload. "directory_listing" isn't even a
muchi-pals concept (no file browser here) - this was stray, forgotten
debug output, not gated behind any flag. Removed both lines, rebuilt
clean. Same two lines were also found (not yet removed) in pal-chain,
pal-forum, and login-signup's own local copies of this file - present
in some local copies, absent from the canonical wsr-pal source and
from pal-chat-irc/muchi-pal-agent's copies. Worth the same 2-line
removal there whenever those projects are next touched - low priority,
purely cosmetic (they don't affect real behavior, just spam stdout).

## 2. REAL, CONFIRMED, NOT YET FIXED: `${panel_content}` was never
   wired into the engine - this is why Tokens/pet stats never show

Root cause, confirmed by direct read, not guessed: `ops/
muchi_compose_frame.c`'s own header comment says its output variable
was deliberately "renamed from the real upstream's own `${game_map}`
name, which misleadingly implied an interactive map/movement concept
this pure-menu project has none of" - and every layout in this project
(`user.chtpm`, `faucet.chtpm`, `store.chtpm`, `pets.chtpm`,
`processes.chtpm`) was updated to use `${panel_content}` instead of
`${game_map}`.

But `system/chtpm_parser_pal.c`'s own "GENERIC VIEW LOADING" block
(~line 1470-1510), which is the code that actually reads `pieces/apps/
player_app/view.txt` (what `muchi_compose_frame.c` writes) and makes
it available to layout substitution, was never updated to match - it
still only does:

    set_var("game_map", map_buf);
    set_var("desktop_view", map_buf);

`panel_content` is a variable name the engine has simply never heard
of. It substitutes to nothing, silently, on every screen. Confirmed
directly: `view.txt` genuinely contains the correct text ("Tokens:
55", a selected pet's real stats, etc.) at the moment a frame renders
- the DATA is right, the OP is right, only the substitution name is
disconnected. This is NOT a blockchain/network issue, and it is NOT
new - it's been broken since whichever earlier session did the
`game_map` -> `panel_content` rename in the layouts without updating
this one line in the engine.

**Not fixed today** - direct user instruction: "dont worry about that
right now, just explain... why its currently stalled." The real,
minimal fix (documented here for whenever it's picked up) is a single
added line: `set_var("panel_content", map_buf);` alongside the
existing two - additive, doesn't touch `game_map`/`desktop_view` (kept
in case anything else still expects those names), lowest possible risk.

## 3. REAL, REPRODUCIBLE, NOT YET ROOT-CAUSED: the "[No Methods]" flash
   on screen transitions

Live-captured directly in the user's own terminal, unmodified baseline
(no prototype module involved): navigating into `pets.chtpm` shows a
correct "P E T S" title immediately, but the very first frame's own
piece_methods list renders as `[No Methods][>] 1. [Back to Main]`
before self-correcting to the real, full pet list one frame later
(milliseconds later, same navigation event - not a hang, not a dead
end, just a real, visible flash).

This is the exact class of bug `main_loop_chtpm.pal`'s own "DOUBLE-
RENDER FIX" comment (2026-07-26) claims was already solved (running
the module's idle-sync once before its first compose, so a freshly-
launched module's first frame is already correct). It clearly is NOT
fully solved under real, human-paced input - my own earlier automated
test (artificial ~0.5-1s delays between injected keystrokes) never
reproduced it, which is itself a real clue: whatever race is causing
this is timing-sensitive in a way slow, evenly-spaced synthetic input
doesn't trigger, but real/faster human typing does.

**Not root-caused yet.** Worth a real investigation (comparing exact
frame timestamps/ordering against a fast, real reproduction) before
anyone tries to fix it - this is exactly the kind of bug where
patching blind is likely to just move the race somewhere else.

## 4. What NOT to do: a full rewrite

Direct question from the user: given how much is broken, should we
just rewrite this instead of patching it, since a bigger per-screen-
module refactor (Phase 2, see doc 04) is already planned anyway?

My recommendation: no.

- §2 (panel_content) and §1 (debug spam) are both small, precisely
  diagnosed, one-or-two-line fixes with a clear, confirmed root cause.
  A rewrite to fix a missing `set_var()` call is enormous overkill and
  strictly more risk than the bug itself.
- §3 (the flash) isn't root-caused yet. Rewriting code before
  understanding WHY it's broken just means writing new code capable of
  reproducing the same unknown race - understanding has to come first
  regardless of whether the eventual fix is a patch or a rewrite.
- The Phase 2 per-screen-module split (doc 04) is a separate, opt-in
  architecture change, not a prerequisite for fixing any of the three
  bugs above, and none of the three bugs are caused by the current
  shared-module architecture - bundling "let's rewrite while we're in
  here" onto an unrelated, already-scoped-as-incremental effort is
  exactly how a bugfix turns into an open-ended rewrite.

## 5. The actual plan, in order

1. DONE: remove debug spam (this session).
2. NOT DONE, LOW RISK, WHENEVER PICKED UP: add
   `set_var("panel_content", map_buf);` to `chtpm_parser_pal.c`'s view-
   loading block - fixes Tokens/pet-stat display everywhere in this
   project in one line.
3. NOT DONE, NEEDS REAL INVESTIGATION FIRST: root-cause the "[No
   Methods]" flash under real/fast input before attempting a fix.
4. SEPARATE, EXPLICITLY PAUSED: the Phase 2 per-screen-module
   prototype (doc 04) - one screen at a time, per direct instruction,
   resume only once the above are settled so the prototype is tested
   against a clean baseline rather than tangled up with pre-existing
   bugs.
