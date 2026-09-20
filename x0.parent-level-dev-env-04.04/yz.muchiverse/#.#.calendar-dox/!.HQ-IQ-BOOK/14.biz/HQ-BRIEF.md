# HQ-BRIEF — the whole HQ on one page

**Purpose:** the compact tech + business snapshot to hand another agent
before a strategy session. Deeper material is linked at the end. Every
business figure here is founder-reported and every business doc is a
`DRAFT` — see `14.biz/`.

---

## 1. What THE HOUSE / HQ is

A **plain-text, file-driven desktop + app platform** you can read,
script, and build games on. Every window, menu, palette, and piece of
app state is a flat file; one shared engine renders all UI from
declarative markup + CSS; app logic runs as small separate processes
that only publish/poll text files. Consequence: every workflow is
inspectable and automatable, and **an AI agent drives any window
through the same input path a human uses**.

"HOUSE" and "HQ" = the whole stack (platform + tooling + games + lore).
Talk to a specific audience → lead with the matching **OUTLET**.

## 2. Architecture in five bullets

- **File-based state.** No hidden binary state. Managers publish
  `<name>_ui.txt`, poll `<name>_action.txt` (`seq=N` / `cmd=…`).
- **One shared renderer** (`khtpm_core_render.c`) draws every window
  mode from `.xhtpm` + CSS via a real flexbox-ish layout engine.
  **Zero per-app C** — house rule.
- **Relay input.** Every window polls a per-PID relay file; real X11
  input and agent-written events share one path → fully agent-drivable.
- **Manager processes** own business logic, one per app; new shared
  glue: `&.widgits/_shared-lib/kh_plat.h` (portable sleep / clock /
  mkdir / script-spawn / signals — no per-manager `#ifdef`).
- **Cross-platform posture:** Linux canonical; macOS runs today via
  XQuartz; Windows = thin shims, not a rewrite. Plan +
  porter-guidance: `08-roadmap/design-docs/CROSS-PLATFORM-SEAM-AND-
  SHARED-INFRA.md`. Keeping it modular is also what makes a piece
  **licensable without selling the whole house**.

## 3. Product surface

- **Platform:** the desktop environment, taskbar, window system.
- **Tooling:** RPG-Maker-style tile + sprite pipeline, a crafting
  system (Canvas-Craft), plain-text + CSV editors, palettes (incl. a
  periodic-table element picker).
- **Games:** a gamified Bible RPG — play good/evil ("cursword"),
  possess Bible characters, progress them through stories, collect /
  trade them; plus original lore-driven titles (see `LUCKY_SOL_PEN`).

## 4. Entity structure  (founder's model — confirm legal shape)

```
JBM-HOLDINGS
  └─ management / holding layer ("MTENT")
       └─ HQ = "Holdings Qompany"  (the tech + product house)
            ├─ OUTLETS  (public-facing brand personas / channels)
            └─ PLATFORM + TOOLING + GAMES
```
`JB.BLOCKROACH.EZ` (security services) is a **revenue line independent
of the platform**. Detail: `14.biz/HOLDINGS-STRUCTURE.md`.

## 5. Outlets  (each a separate public "entity"; founder-reported X/Twitter state)

| Outlet | Audience | Followers | Tone / notes |
|---|---|---|---|
| **TSOTS** | Bible / spiritual; gamified-Bible players; maybe sports/politics/natsec | small | reverent-playful; lore LOW |
| **Tear-it co** | RPG Maker players & devs; EN otaku; JP-speaking games | ~30, "loyal" | dev-to-dev, bilingual; lore light |
| **Harnecient** | cutting-edge AI / harness users; investors | <25 | technical; investor story = agent-native platform; lore OFF |
| **Robot Trap House** | mainstream lowbrow gaming+tech news, retro, 90s | ~50 (most) | meme/gif "slop"; **impersonal / secret**, not founder's name |
| **TEMPT** | asian / female / occult fashion / aspirational spaces / scifi-fantasy | <25, high engagement | aesthetic, aspirational; slop posts perform |
| **JB.EZ** | founder personal; warm network | <25 | "Mr. Robot" flavor; hacker-alter-ego lore light; real connections land here |
| **JB.BLOCKROACH.EZ** | security-services clients & community | <25 | professional, real-name, no lore; own pricing/sales track |
| **JBM** | academic peers | — | scholarly; BS:CySec + MBA-ITM; politics/academia; firewalled from slop/lore |
| **LUCKY_SOL_PEN** | insider / lore fans | <25, "secret/cringe" | founder-as-celebrity extreme; **primary game-lore source** |

Per-outlet detail: `14.biz/OUTLETS/<NAME>/00-INDEX.md`.

## 6. Cross-cutting branding + the open strategic question

Shared founder-lore across most outlets: **SF/NY/(Japan) founder-devs**,
targeting single asian females, cyberpunks, "redheads" (red-head
vampire-clone lore), a "Tenchi Muyo"-style ensemble of familiar
female-assistant characters. **This is branding, not a product claim.**

Founder persona ladder: `JB.EZ` (grounded "Mr. Robot") → `LUCKY_SOL_PEN`
(extreme: space, time travel, Mars clone army, "man of mystery").

**The question to strategize:** *when to lean INTO the lore vs. distance
from it* — on a sales call? by demographic? cold vs. warm audience?
Wanted output: a `context × audience → lore level (none/light/full)`
table. Also unresolved: which accounts may "slop post", how the secret
accounts route attention to the real product without breaking their
separation. See `14.biz/OUTLETS/_ALL-cross-cutting.md`,
`14.biz/STRATEGY/{sales,marketing,ethics}.md`.

## 7. Strategy areas (all `DRAFT` stubs in `14.biz/STRATEGY/`)

`sales` · `marketing` · `pricing` (model not set — free/tiers/services/
game sales) · `networking` · `legal` (entity shape, IP, FOSS vs.
proprietary split — needs counsel) · `ethics` (data/AI use,
targeting lines, lore boundaries) · `strategy` (6–18mo priorities,
sequencing outlets vs. platform vs. games).

## 8. Long-horizon north star (`14.biz/FUTURE-GOALS.md`)

AI-bot-driven operations; character bots on the social platform;
sell/license pieces without selling the house; run multiple businesses
(cloud / enterprise / cybersec); stated ambition "bigger than Elon";
someday biotech + space; founder may add law school or an MD. **Not
near-term — do not let these pull scope.**

---

## Deeper reading
- Tech: `01-orientation/`, `02-architecture/CENTROID_GOLD_STD.md`,
  `08-roadmap/00-INDEX.md`
- Biz: `14.biz/00-INDEX.md` (nav), `14.biz/BIZ-BOOK.html` (one-page
  human view), `14.biz/user-biz-request.txt` (founder intake, raw)
- Marketing deck scaffold: `08-roadmap/design-docs/MARKETING-PRESENTATION-OUTLINE.md`
