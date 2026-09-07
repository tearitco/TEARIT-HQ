# 14 — Biz

Business, strategy, and outreach. Deliberately kept OUT of the
engineering chapters: everything here is **founder-owned**,
market-facing, and moves on a different clock than the code. Agents
organize and draft; the founder decides. Do not invent pricing, legal
posture, financials, or traction numbers — capture what the founder
actually said and mark the rest `DRAFT` / `TBD`.

**Raw intent lives in `user-biz-request.txt`** — the founder's intake
note. Append new asks there; then fold them into the structured docs
below.

---

## How to read this section (start shallow, go deeper only as needed)

| You want… | Go to |
|---|---|
| the one-liner / 30-sec pitch for **THE HOUSE / HQ** | `ELEVATOR-PITCH.md` — nothing deeper needed |
| who owns what (entity stack) | `HOLDINGS-STRUCTURE.md` |
| to pitch **one specific audience** | `OUTLETS/<NAME>/00-INDEX.md` — that outlet's persona, audience, tone, follower state, and its tailored pitch |
| the **shared** founder-lore + when to lean in vs. pull back | `OUTLETS/_ALL-cross-cutting.md` |
| company-wide **sales / marketing / pricing / networking / legal / ethics / strategy** | `STRATEGY/00-INDEX.md` → the per-area file |

"HOUSE" and "HQ" are used as the all-encompassing umbrella term for the
whole technology + product + lore stack. Use them for general pitches;
use an **OUTLET** name when talking to that outlet's specific
demographic.

---

## Layout

```
14.biz/
  ELEVATOR-PITCH.md         HOUSE / HQ — short, and shorter (top-level, easy grab)
  HOLDINGS-STRUCTURE.md     JBM-HOLDINGS -> mgmt entity -> HQ -> subsidiaries/outlets
  FUTURE-GOALS.md           long-horizon founder ambitions (north star, not commitments)
  BIZ-BOOK.html             the WHOLE of 14.biz rendered as one readable page (regenerate after edits)
  user-biz-request.txt      founder intake — RAW, append-only, do not restructure

  STRATEGY/
    00-INDEX.md
    sales.md      marketing.md   pricing.md     networking.md
    legal.md      ethics.md      strategy.md

  OUTLETS/
    _ALL-cross-cutting.md   shared SF/NY/Japan founder-lore; lean-in / lean-out rules
    TSOTS/                   bible / spiritual + gamified bible ("cursword")
    TEAR-IT-CO/              RPG Maker players & devs, EN otaku + JP-speaking games
    HARNECIENT/              cutting-edge AI tools / harness users / investors
    ROBOT-TRAP-HOUSE/        mainstream lowbrow gaming+tech news, retro, 90s nostalgia
    TEMPT/                   asian / female / occult fashion / aspirational spaces / scifi-fantasy
    JB-EZ/                   founder/dev persona, "mr. robot" flavor, personal + networking
    JB-BLOCKROACH-EZ/        founder's pentest / bug-bounty / red+blue-team services co.
    JBM/                     academic account (BS:CySec + MBA-ITM), politics / academia
    LUCKY-SOL-PEN/           founder-as-celebrity lore extreme; most game lore derives here
    CONSULTING/              paid advisory + build for law firms / biotech;
                             incl. the file-lineage-blockchain offer + a
                             tech-readiness assessment against 041.pal-chain
```

## Sub-division convention

Each `OUTLETS/<NAME>/` starts as a single `00-INDEX.md` (persona +
audience + tone + follower state + cross-promo notes + a `## Tailored
pitch (DRAFT)` section). Split out `tailored-pitch.md`,
`content-strategy.md`, or an outlet-level `pricing.md` / `networking.md`
**only when that outlet's answer actually diverges** from the
company-wide `STRATEGY/` default. Company-wide doc is the default;
outlet files are overrides.

## One-file views

- **Human, whole biz section:** `BIZ-BOOK.html` — open in a browser.
  It is generated from the `.md` files here; after editing any of
  them, regenerate it (`../tools/build-biz-book.sh`, or ask an agent).
- **Agent, whole HQ (tech + biz) in one compact page:**
  `../HQ-BRIEF.md` at the book root — the thing to paste to another
  agent when strategizing.

## Status

Scaffold created 2026-09-06 (see `12.calendar/2026-09-06/`). Every doc
below is a stub carrying the founder's stated intent verbatim, marked
`DRAFT`. No business claims have been invented.

## Related (engineering side, feeds marketing copy)
- `08-roadmap/design-docs/MARKETING-PRESENTATION-OUTLINE.md`
- `10-user-docs/` — user-facing product docs
- `01-orientation/` — what the house actually is, technically
- `LUCKY-SOL-PEN` lore source (external drive, not in repo):
  `…/4.🎓️capstone…/13.Soul_Pen_FulPitch]….md` — path in
  `user-biz-request.txt`
