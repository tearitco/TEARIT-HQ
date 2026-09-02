# 07 — Install and ship

- `PHONDO_INSTALL_IDEAS.md` — install/versioning/store/CLI-bootstrap
  ideas. Moved verbatim from `#.#.calendar-dox/2.SEPT/`.
- `S1_HOUSE_PATH_MIGRATION.md` — see `09-appendix/` (kept there since
  it's the definitive record of the 2026-09-01 path migration, which
  this book's own README already cross-references).

**Not found in this pass**: `xyz-installer-dev/dev-doc/` (versioning,
rollout channels, hardcoded-path-fragility, Harnecient fresh-install
design) was referenced by several older docs
(`0.browser-prompting/project-starters/15.xyz-installer-dev-
exploration-delegation.md` has the fullest pointer: real location
`x0.parent-level-dev-env-04.04/xyz-installer-dev/`, sibling to this
`yz.muchiverse/` dir) but does not exist under `yz.muchiverse/` itself
— it lives one level up in the dev-env tree, outside this migration's
scope. Worth checking directly if install/ship work resumes.

- `USER-JOURNEY-COMPLETION-GRAPH.md` — the full real user journey,
  install through publishing a toy to the store, as a step-by-step
  completion graph (what's real today vs. genuinely new work, checked
  against actual source per step, not assumed). Start here for "what's
  actually left before this ships."
- `SECURITY.md` — **long-term security concerns for shipping to
  strangers** (2026-09-02): install-script trust, store-content
  supply-chain risk, network-fetched-content risk (SSRF, fetch limits,
  media-decode CVEs, arbitrary JS execution), and account/auth gaps.
  Written after the network-browser branch review surfaced a planned
  Duktape JS eval op as a real, concrete case needing this exact
  analysis. See `02-architecture/HTML-MEDIA-AND-SCRIPTING.md` for the
  rendering-side design its §4 concerns apply to.
