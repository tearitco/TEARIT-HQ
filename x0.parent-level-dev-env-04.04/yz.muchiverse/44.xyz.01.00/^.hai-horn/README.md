# HORN_CHAT Handoff for Kilo

**Status:** Ready for handoff  
**Date:** 2026-10-01  
**To:** Kilo (low-context, high-capability agent)

---

## Start here

1. **New to this task?** Read `dox/00-QUICK-START.md` (5 min)
2. **Ready to build?** Read `HORN_CHAT-HANDOFF.md` (15 min)
3. **Need code references?** Read `dox/01-CODE-REFERENCES.md`
4. **Questions about decisions?** Read `dox/02-DECISIONS-AND-TESTING.md`

---

## The mission (one sentence)

Build a terminal-based CLI chat harness (`HORN_CHAT`) using OpenRouter API models, reusing chtpm primitives from gem-dev, as the foundation for a mini attrition-model pipeline.

## Three sprints

- **Sprint 1:** HORN_CHAT v0.1 — basic chat + text completion
- **Sprint 2:** HALO_CHAT v0.1 — add Concept Bank validation
- **Sprint 3:** IRL design — learning architecture (design doc only, no code)

## Key decisions to raise with owner

1. Should HALO's validated edits go to a review file or direct to the bank?
2. How should the IRL harness grade model outputs (self-judge or separate judge)?
3. Should chat history be persistent across sessions?

See `dox/02-DECISIONS-AND-TESTING.md` for details.

---

## File structure

```
^.hai-horn/
├── README.md (this file)
├── HORN_CHAT-HANDOFF.md (main handoff — read this first)
├── dox/
│   ├── 00-QUICK-START.md
│   ├── 01-CODE-REFERENCES.md
│   └── 02-DECISIONS-AND-TESTING.md
└── 2do-TMP_0.0001.txt (original owner request)
```

After you start building:

```
^.hai-horn/
├── ops/
│   ├── horn_chat_openrouter.c (copy of ai_chat_openrouter.c)
│   ├── horn_chat_chtpm_parser.c (copy of chtpm_parser.c)
│   ├── halo_chat_describe.c (copy of ai_describe.c)
│   └── halo_chat_validate.c (copy of concept_edit_validate.c)
├── layouts/
│   └── horn_chat.xhtpm (terminal layout for chat window)
├── horn_chat.pal (main loop for HORN)
├── horn_chat.sh (launcher for HORN)
├── halo_chat.pal (main loop for HALO)
└── halo_chat.sh (launcher for HALO)
```

---

## One thing before you start

**You have everything you need.** This is not a "figure it out as you go" handoff. All code that exists is linked. All decisions that are still open are flagged as such.

Read `dox/00-QUICK-START.md` first, then `HORN_CHAT-HANDOFF.md`, then start building.

**Blockers?** They're documented in `dox/02-DECISIONS-AND-TESTING.md`. Raise them with the owner.

---

**Ready? Go to `HORN_CHAT-HANDOFF.md`.**
