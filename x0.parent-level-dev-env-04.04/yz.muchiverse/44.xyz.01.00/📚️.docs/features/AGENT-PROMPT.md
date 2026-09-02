# Feature Documentation Guide for Future Agents

When documenting a new feature, create or update a markdown file in `/📚️.docs/features/` following this template. This keeps feature docs localized, searchable, and maintainable.

## When to Create Feature Docs

- After completing a new user-facing feature
- When a feature reaches MVP (minimum viable product) status
- When planning multi-phase work (document current + planned phases)
- When integrating features together (cross-reference in related sections)

## Template Structure

```markdown
# Feature Name

**Status**: [MVP | In Progress | Planned | Deprecated | Stable]

## Overview
(1-2 paragraphs describing what users can do with this feature)

## Architecture

### Components
(List each component with path and brief description)

### User Flows
(Step-by-step walkthrough of how users interact with feature)

## Files Modified/Created
(List all files touched, with path and change type: NEW/MODIFIED)

## Future Enhancements
(Checkbox list of planned improvements)

## Testing

### Manual Testing Checklist
(Steps to verify the feature works)

### Known Issues
(Current bugs or limitations, if any)

## Related Features
(Links to other feature docs using [[feature-name]] syntax)
```

## Key Guidelines

1. **Status Field**: Keep it accurate. "MVP" means it works but may have rough edges. "In Progress" means actively being built. "Stable" means production-ready.

2. **Paths**: Use the symlink shortcuts (e.g., `&.widgits/open-hai`, `%.harnesses`, `#.monads`) rather than full paths. Makes docs resilient to directory renames.

3. **Architecture**: Explain HOW the feature works, not just WHAT it does.
   - What files are involved?
   - How do they communicate?
   - What's the execution flow?

4. **User Flows**: Write step-by-step from the user's perspective, not internal implementation.

5. **Testing**: Include concrete steps an agent (or human) can use to verify the feature works.

6. **Cross-linking**: Use `[[feature-name]]` to reference other features. It helps future agents understand the ecosystem.

7. **Known Issues**: Be honest about limitations. This helps future agents understand scope and avoid duplicate work.

## Example Commands for Future Agents

### Search for an existing feature doc
```bash
ls -la /📚️.docs/features/
grep -r "some-keyword" /📚️.docs/features/
```

### Find files related to a feature (for "my-feature")
```bash
grep -r "my-feature\|my_feature" . --include="*.c" --include="*.sh" --include="*.md"
```

### View what's documented vs. what's undocumented
```bash
ls /📚️.docs/features/ | while read f; do echo "$f: $(head -1 "$f")"; done
```

## Common Patterns

### Feature with UI Component
Include sections on:
- Navigation integration (enum constants, menu items)
- Event handlers (button clicks, keyboard shortcuts)
- Styling/rendering code
- Asset locations (icons, templates)

### Feature with Background Service
Include sections on:
- Process launch/supervision
- IPC mechanism (sockets, pipes, files)
- Lifecycle (startup, shutdown, restart)
- Logging locations

### Feature with Data Storage
Include sections on:
- Directory structure
- File format (plain text, binary, JSON)
- Update frequency
- Retention policy

## How to Maintain Docs

- Update "Status" field as feature matures
- Add discovered issues to "Known Issues"
- Move completed tasks from "Future Enhancements" to notes about when they were done
- Add links to new related features as they emerge

---

**Last updated**: 2026-08-13  
**Created by**: Agent documentation standardization task
