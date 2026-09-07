# THE HOUSE / HQ — elevator pitch  (DRAFT — founder to refine)

Kept at the top of `14.biz/` on purpose: grab-and-go copy for a quick
pitch without digging. Longer, audience-specific versions live in
`OUTLETS/<NAME>/`.

---

## One line

**THE HOUSE is a plain-text, file-driven desktop + app platform you can
read, script, and build games on — where every window, menu, and piece
of state is a file, and an AI agent is a first-class user.**

## ~15 seconds

THE HOUSE (a.k.a. HQ) is a self-contained desktop environment where the
whole UI — windows, taskbar, menus, palettes — is rendered from
declarative text files by one shared engine, and every bit of app state
is a flat file you (or an AI, or another program) can read and write.
That makes it scriptable end to end, easy to automate, and a natural
base for tools and games.

## ~30 seconds

Most software hides its state inside a binary. THE HOUSE turns it
inside out: menus and layouts are declarative markup + CSS, app logic
runs as small separate "manager" processes that just publish and poll
text files, and one generic renderer draws all of it. The payoff —
every workflow is inspectable and automatable, an AI harness can drive
any window through the same input path a human uses, and new apps or
games are assembled from a shared vocabulary instead of built from
scratch. On top of that platform we ship tools (RPG-Maker-style tile
and crafting tooling, editors) and games (a gamified Bible RPG, plus
original lore-driven titles).

## The umbrella

"HOUSE" / "HQ" = the whole stack: the platform + the tooling + the
games + the lore. When the audience is specific — RPG Maker devs, AI /
harness users, Bible-game players, etc. — lead with the matching
**OUTLET** instead (see `OUTLETS/`).

## What it is NOT (keep pitches honest)
- Not shipping at scale yet — pre-traction; outlets are small (see each
  `OUTLETS/<NAME>/00-INDEX.md` for real follower counts).
- Not a cloud service — it's a local-first environment.
- Cross-platform (Windows / macOS) is designed-for, Linux-first today
  (`07-install-and-ship/windows-mac/`).

---

### TODO (founder)
- Pick the single sharpest one-liner; the rest hang off it.
- Decide how much lore (`LUCKY-SOL-PEN`, the "familiar female
  assistants" branding) belongs in a cold pitch vs. warm/insider
  contexts — cross-ref `OUTLETS/_ALL-cross-cutting.md`.
- A dollars-and-cents "why this matters to a buyer/investor" line per
  audience → `STRATEGY/sales.md`.
