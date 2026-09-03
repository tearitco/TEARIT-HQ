# 02 — Architecture

- `CENTROID_GOLD_STD.md` — **the current gold-standard rendering
  architecture.** Read this before touching any khtpm-family
  renderer/manager code. Moved verbatim (`git mv`) from
  `44.xyz.01.00/CENTROID_GOLD_STD.md`.
- `RENDERING-ORIENTATION.md` — how the khtpm merged binary actually
  works today (the "current, not-yet-ideal" companion to the gold
  standard).
- `TWO-PARSER-FAMILIES.md` — why chtpm_parser_pal and khtpm both exist,
  at a glance, and current migration posture.
- `INPUT-RELAY-PIPELINE.md` — how a click/keypress reaches the
  renderer, the relay files, nav_index/Tab-cycle design, LayDoc vs
  Elem/CSS.
- `STATE-AND-PDL-CONVENTIONS.md` — file-based state discipline, what a
  "manager" process is, PDL conventions, asset-path conventions.
- `LEGACY-GL-PIPELINE.md` — the older chtpm_parser_pal → GL mirror
  pipeline, for projects still on that path.
- `HTML-MEDIA-AND-SCRIPTING.md` — should a browser-type app get a
  separate HTML-specific parser, new img/video tag types, or reuse
  the existing generic `sprite=` mechanism? (Reuse it — no new parser,
  no new tags.) Also covers where Duktape JS execution should live
  (manager/op-side only, never the shared renderer) and cross-refs
  `07-install-and-ship/SECURITY.md` for the sandbox questions that
  raises.

Condensed from: `44.xyz.01.00/CENTROID_GOLD_STD.md` (moved),
`#.#.calendar-dox/1.^V-hq/SKILLS.md` §2, `HOUSE_FAQ.md` (Architecture/
Files-Compliance/Nav-Input/Assets sections), `44.xyz.01.00/
!.HOUSE_STDS.md` §A.7/§B.
