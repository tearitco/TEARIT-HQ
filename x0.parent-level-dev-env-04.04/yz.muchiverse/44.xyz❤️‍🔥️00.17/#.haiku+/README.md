# #.haiku/ — Haiku Agent Context Directory

This directory contains all context, playbooks, and working files for Haiku (Claude Haiku). It's split into two folders so both the user and the agent know what they own, what they're responsible for updating, and what they can safely ignore.

## Directory Structure

### for-user/
Files YOU (the user) own and update. Haiku reads these to understand priorities, context, and your preferences. You review and correct Haiku's understanding of you.

- **user.txt** — Your profile, collaboration style, standing conventions, known gotchas. Haiku reads this cold every session. **You review this and correct it if Haiku misunderstood.**
- **CURRENT_SESSION_PRIORITIES.txt** — Updated by YOU at the start of each session. Main focus, subgoals, blockers, context. Haiku reads this to know what you're actually working on RIGHT NOW.
- **index_context_summary.md** — Quick reference for the ecosystem (projects, files, conventions, the ONE pattern). Haiku reads this on cold start. You can update if things change significantly.
- **SCALE_BANK_draft.txt** — Prep work for section 4.6 (future feature). You write/iterate this, Haiku reads when ready to build.

### for-agent/
Files Haiku owns and updates. You can read these if curious, but they're Haiku's working directory.

- **agent.txt** — Haiku's cold-start playbook. What to load first, how to escalate, when to ask, when to just decide. Haiku reads this to know how to behave. **Updated by Haiku only.**
- **sonnet-handoff.txt** — Template for handing off genuinely hard tasks to Sonnet. Only created when needed. **Haiku writes it when stuck, suggests it to you.**
- **gotchas_by_project.txt** — Haiku's persistent notes. Every project-specific discovery goes here immediately. Grows over time as Haiku learns the codebase. **You can read this (it's useful!), but Haiku owns updates.**

## How Haiku Uses This Directory

### On Cold Start (Every Session)
1. Load for-user/user.txt (who you are, how you work)
2. Load for-user/CURRENT_SESSION_PRIORITIES.txt (what we're doing today)
3. Load for-user/index_context_summary.md (quick reference)
4. Load for-agent/agent.txt (my playbook)
5. Check for-agent/gotchas_by_project.txt (any traps I've found before)
6. Proceed with work

### During Session
- Update for-agent/gotchas_by_project.txt immediately when discovering project-specific gotchas
- Update for-user/CURRENT_SESSION_PRIORITIES.txt progress notes if priorities shift
- Create for-agent/sonnet-handoff.txt ONLY if genuinely stuck (and ask you before using it)

### End of Session
- Suggest updating for-user/CURRENT_SESSION_PRIORITIES.txt (mark subgoals done, add new blockers)
- Haiku updates for-agent/gotchas_by_project.txt if anything new was discovered
- Delete for-agent/sonnet-handoff.txt if it was created (was only for that session)

## What Haiku Won't Do

- Re-read for-user/user.txt every time (loaded once per cold start)
- Ask "should I proceed?" on already-decided items
- Ignore gotchas just because they were found in a prior session
- Update for-user/ files (that's your job)
- Ask permission for reversible local work (file edits, git commits in own branches)

## What Haiku Will Do

- Load context files at the start of every session
- Ask for clarification ONLY if something in for-user/ is genuinely ambiguous
- Escalate to Sonnet if a task genuinely needs Sonnet (via sonnet-handoff.txt)
- Update for-agent/gotchas_by_project.txt the moment something new is discovered
- Suggest updates to for-user/CURRENT_SESSION_PRIORITIES.txt if priorities shift mid-session

## Tips for Maximum Effectiveness

1. **Keep CURRENT_SESSION_PRIORITIES.txt fresh.** If you change your mind mid-session, update it. Haiku will re-read it if you tell them it changed.
2. **Let gotchas_by_project.txt grow.** It's Haiku's long-term memory. The bigger it gets, the fewer mistakes Haiku will repeat.
3. **Review user.txt occasionally.** If Haiku misunderstood your preferences, correct it. This teaches Haiku how to work with you better.
4. **Use sonnet-handoff.txt as a safety valve.** It's not a cop-out — it's how Haiku knows when a task genuinely needs bigger reasoning.
5. **Don't worry about for-agent/ files.** They're working files. Haiku manages them. You can read them for curiosity, but you don't need to touch them.

## Files Not Here (But Related)

- **zest-er-summary.txt** — Full session context from the previous build. Haiku reads this once per session if recent. Lives in parent directory (2.muchi-verse-0.0/).
- **!.xyzos-standards.txt** — Running rulebook for the entire ecosystem. Haiku reads this for any project-specific work. Lives in parent directory.
- **CURRENT_SESSION_PRIORITIES.txt** — You could also keep this in the parent directory if you prefer. Haiku will look for it here FIRST, then in parent if not found.

---

**Last updated:** 2026-07-20  
**Status:** Ready for use
