# HORN_CHAT — Kilo progress

**Status:** Sprint 1 complete (verified 2026-10-05)  
**Date:** 2026-10-01 (handoff) / 2026-10-05 (first working build)

---

## What's done

- `ops/horn_chat_openrouter.c` — clean build, zero warnings, verified OpenRouter round-trip
- `horn_chat.sh` — launcher with stdin main loop, `@` completion, `q` to quit
- `horn_chat_test.sh` — non-interactive round-trip test, writes `.horn-sessions/test_report.txt`
- `dox/03-RELAY-AND-TOOLCALLS.md` — documents relay output and future tool-call roadmap
- `dox/02-DECISIONS-AND-TESTING.md` — includes future-testing desire for driving `toys:20.DSR` via relay injection

## What's next

1. Integrate `chtpm_parser.c` rendering into the main loop (layout file + `.pal` loop)
2. Sprint 2: HALO_CHAT v0.1 with Concept Bank validation
3. Future: test hai-horn as an autonomous driver for WSR toys:20.DSR via relay injection

## File structure (current)

```
^.hai-horn/
├── README.md (this file)
├── HORN_CHAT-HANDOFF.md (original handoff)
├── horn_chat.sh (launcher)
├── horn_chat_test.sh (round-trip test)
├── dox/
│   ├── 00-QUICK-START.md
│   ├── 01-CODE-REFERENCES.md
│   ├── 02-DECISIONS-AND-TESTING.md
│   └── 03-RELAY-AND-TOOLCALLS.md
├── ops/
│   └── horn_chat_openrouter.c (OpenRouter op, builds clean)
├── layouts/
│   └── horn_chat.chtpm (terminal chat layout)
└── .horn-sessions/ (runtime state, gitignored)
```

---

**Next:** See `HORN_CHAT-HANDOFF.md` for Sprint 2 (HALO_CHAT) and Sprint 3 (IRL design).
