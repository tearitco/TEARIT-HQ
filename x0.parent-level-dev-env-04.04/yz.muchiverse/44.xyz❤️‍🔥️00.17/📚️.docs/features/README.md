# Feature Documentation

This directory contains user-facing feature documentation. Each feature is documented in its own markdown file with architecture, user flows, testing steps, and future enhancements.

## Directory Structure

```
features/
├── README.md (this file)
├── AGENT-PROMPT.md (guide for agents documenting new features)
└── *.md (individual feature documentation)
```

## Current Features

- **[Per-Session Stats](per-session-stats.md)** — View delegation metrics for each hai chat session. Shows all sessions as tabs with metrics like user messages, AI responses, tool calls, and delegation rate.

## How to Document a New Feature

1. Read [AGENT-PROMPT.md](AGENT-PROMPT.md) for the template and guidelines
2. Create a new `feature-name.md` file in this directory
3. Follow the template structure: Overview, Architecture, Components, User Flows, Files Modified, Testing, Future Enhancements
4. Use symlink shortcuts for paths (`&.widgits`, `%.harnesses`, etc.)
5. Add cross-references to related features using `[[feature-name]]` syntax
6. Update this README with an entry for the new feature

## Searching for Feature Docs

Find all features or search for specific topics:

```bash
# List all documented features
ls -la /📚️.docs/features/*.md

# Search for a keyword across all features
grep -r "some-keyword" /📚️.docs/features/

# Find files related to a feature
grep -r "feature-name\|feature_name" . --include="*.c" --include="*.sh"
```

## Integration Points

Features may appear in multiple places:

- **Hai menu** — Menu items in the chat interface sidebar
- **Toolbar** — Buttons in the taskbar application
- **Stats** — Delegation tracking and reporting
- **Settings** — Configuration options for user preferences
- **Documentation** — User guides and feature references

## Status Levels

- **Planned** — Designed but not yet implemented
- **In Progress** — Actively being built
- **MVP** — Minimum viable implementation, may have rough edges
- **Stable** — Production-ready, tested and maintained
- **Deprecated** — No longer recommended, kept for compatibility

---

**Last updated**: 2026-08-13
