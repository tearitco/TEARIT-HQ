# Maintenance Fixes — Small, Non-Blocking Polish Items

Running list of small UI/UX issues noticed during other work — not blocking, fix when convenient
or bundle into a polish pass.

---

## khtpm livedesk taskbar

### USER cell — New User row has no index number
**Observed:** 2026-08-11, live testing of the new USER cell submenu.
**Issue:** Submenu rows built by `livedesk_build_session_menu()`/`livedesk_build_desk_menu()` etc.
show a `[N]` index prefix in front of each row's label (matching other popups in this house), but
`livedesk_build_user_menu()`'s rows ("New User...", switch-user entries, "Logout") don't show one.
**Why it matters:** Users select rows by typing the row's digit — without a visible index, they have
to guess/count instead of read it off the screen, inconsistent with every other submenu in this
taskbar.
**Likely fix:** Check how index prefixes get rendered for other hq_menu-based popups (probably in
the layout/parser rendering path, not the manager's `HQMenuItem.label` itself — confirm before
assuming it's a manager-side string-formatting fix) and match that convention.

### USER cell submenu — window width doesn't fit its text
**Observed:** 2026-08-11, same session.
**Issue:** The USER submenu popup window is too narrow for row text like
`"claude-0001 (AgentClaude0001)"` — text gets clipped/wrapped awkwardly.
**Why it matters:** Cosmetic, but makes account names hard to read once display names get longer.
**Likely fix:** Check popup width calculation in khtpm_strip_parser.c (probably computed from
longest-row-text at render time, same as other dynamic popups) — may need a wider minimum or
per-content sizing specific to this cell.

---

## Backlog / Not Yet Investigated

(Add new items above this line as they're noticed — keep one dated entry per issue, don't batch
vague items.)
