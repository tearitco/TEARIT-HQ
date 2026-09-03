# 06 — Testing

- `TESTING_STRATEGY.md` — the relay-only testing rule (no direct CLI
  calls), harness template, frame-history reading. Moved verbatim.
- `HARNESS-AUTHORING-GUIDE.md` — the canonical doc to read before
  building/updating any test/demo harness. Moved verbatim.
- `AIGENT-TESTING-K9.txt` — actively-maintained twin of
  `TESTING_STRATEGY.md`: per-program-family relay contracts, the
  text-relay → text-state-dump → PNG-last ordering, presentation-video
  archive convention. Moved verbatim (read alongside
  `TESTING_STRATEGY.md`, not instead of it).
- `CPU-AND-SESSION-SAFETY.md` — CPU-safety discipline, headless
  testing technique, pixel-verification-without-a-screen. Condensed
  from `!.HOUSE_STDS.md` §C.

**The house's standing rule: relay-file injection first, cheap text
state dumps second, PNG/xdotool only as last resort.** See
`02-architecture/INPUT-RELAY-PIPELINE.md` for the relay files
themselves.
