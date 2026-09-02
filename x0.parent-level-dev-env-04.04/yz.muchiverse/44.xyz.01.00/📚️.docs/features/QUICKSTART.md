# Feature Documentation Quick Start

**Location**: `/📚️.docs/features/`  
**Last Updated**: 2026-08-13

This directory contains user-facing feature documentation and a guide for agents to document new features.

## For Users: Learn About Features

Start here to understand what features are available and how they work.

| Document | Purpose |
|----------|---------|
| [README.md](README.md) | Overview of features directory and search tips |
| [Per-Session Stats](per-session-stats.md) | View metrics for individual hai chat sessions |

### To Find a Feature
```bash
# List all documented features
ls /📚️.docs/features/*.md

# Search for a keyword
grep -r "keyword" /📚️.docs/features/
```

## For Agents: Document New Features

Start here if you need to create or update feature documentation.

### Step 1: Read the Template
Open [AGENT-PROMPT.md](AGENT-PROMPT.md) — it contains:
- Template structure for feature docs
- When to document features
- Key guidelines and best practices
- Common patterns and examples

### Step 2: Create Feature Doc
1. Create new file: `feature-name.md` in this directory
2. Follow template from AGENT-PROMPT.md
3. Use symlink shortcuts for paths (`&.widgits`, `%.harnesses`, etc.)
4. Include: Overview, Architecture, Components, User Flows, Testing
5. Add cross-links using `[[related-feature]]` syntax

### Step 3: Update the Index
Add one line to [README.md](README.md) under "Current Features":
```markdown
- **[Feature Name](feature-name.md)** — One sentence description.
```

## Implementation Reference

[IMPLEMENTATION-SUMMARY.md](IMPLEMENTATION-SUMMARY.md) shows how the per-session stats feature was built as an example of:
- Multi-component system design
- Retroactive data calculation
- UI integration across different systems
- Documentation patterns

Use this as a reference for documenting similar features.

## Documentation Ecosystem

```
Features (user-facing functionality)
├── README.md (feature index)
├── AGENT-PROMPT.md (how to document)
├── QUICKSTART.md (this file)
├── IMPLEMENTATION-SUMMARY.md (example)
├── per-session-stats.md (example feature)
└── [future features...]

These docs support the main system:
├── &.widgits/open-hai/ (hai chat UI)
├── &.hq-apps/stats-hq/ (stats dashboard)
├── %.harnesses/harnecient-fsm/ (delegation tracking)
└── *.monads/*.livedesk-taskbar/ (toolbar/process mgmt)
```

## Common Tasks

### "How do I use feature X?"
→ Read the feature's markdown file in this directory

### "Which features exist?"
→ See [README.md](README.md) "Current Features" section

### "How do I document a feature?"
→ Read [AGENT-PROMPT.md](AGENT-PROMPT.md)

### "What's the complete architecture of feature X?"
→ See the feature's "Architecture" section and cross-linked features

### "How do I test feature X?"
→ See the feature's "Testing" section with manual checklist

## Status Meanings

When you see these in feature docs:

- **Planned** — Designed, not yet built
- **In Progress** — Actively being developed
- **MVP** — Minimum viable implementation (may have rough edges)
- **Stable** — Production-ready, fully tested
- **Deprecated** — No longer recommended

## Tips for Future Agents

1. **Keep docs close to code** — Feature docs live with the feature, not in a separate system
2. **Link everything** — Use `[[feature-name]]` to help users discover relationships
3. **Be honest about status** — "In Progress" is better than claiming stability
4. **Include testing steps** — Verify-able documentation is more trustworthy
5. **Use templates** — The structure in AGENT-PROMPT.md scales across many features

---

**Created by**: Feature documentation standardization (2026-08-13)  
**Maintained by**: Future agents adding new features
