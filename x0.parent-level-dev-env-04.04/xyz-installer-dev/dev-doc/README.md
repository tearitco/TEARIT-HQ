# 🔧 dev-doc — Ongoing Development Documents

> **Working** documents for the team (humans + agents). These change often.
> For stable, audience-facing truth, see `../xyz-official-documentation/`.
> Rule: update the doc when the decision changes — nothing here is final.

## 📚 Index

| Doc | What |
|---|---|
| `01.versioning-with-gitlet🌳🪵️.md` | **Versioning strategy** — how gitlet manages updates, channels, promotion. The big one. |
| `02.rollout-channels-alpha-beta-stable.md` | Rollout policy — what alpha/beta/stable MEAN and the gate to promote. |
| `03.hardcoded-path-fragility-and-portability.md` | **Real bug writeup** — hardcoded/fixed-depth path derivation broke when an entity's install location changed (dev-tree → pals registry). Same failure class will hit multi-machine install directly; includes a practical checklist + a flagged, not-yet-designed external-asset-resolution problem. |
| `04.harnecient-fresh-install-design.md` | **HARNECIENT fresh-install design** (khtpm era) — the one-script install that replaces obsolete install-v1: installed layout, payload, fresh-state policy, launcher, relay-only KPI harness, build phases, risks. **Product name HARNECIENT** (HQ/HAI-Q/hi-iq/haiku all taken). |

## 🗓️ Status

- ✅ Install v1 proven (KPI#4/#5). See sprint doc `2&3-jul31-sprint.md` Phase II. **OBSOLETE** — predates khtpm taskbar.
- 🚧 **HARNECIENT fresh install** — design written (`04.harnecient-fresh-install-design.md`), **code not started**. Working offline-first so a disconnect doesn't lose the plan.
- 🚧 gitlet versioning — **strategy drafted, not wired.** This is a proposal for review.
- ⏭️ Next wave: **agentic AI systems capabilities** (back to `045.muchi-pal-agent🤖️+1`).
