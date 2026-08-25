# Handoff: Wildcard Completion for `/model` Command
**From:** Haiku (Claude Haiku 4.5)  
**To:** Sonnet (Claude Sonnet)  
**Date:** 2026-07-20  
**Context:** User requested `/model *` completion and `/model g*` prefix matching for muchi-pal-agent

---

## What We Have

✅ **Basic `/model` listing works** (implemented 2026-07-20):
- User types `/model` (no args) → Lists all available models
- Reads from `pieces/registry/models/model_list.txt`
- Outputs as a tool response to chat

✅ **Model switching works**:
- `/model <id>` switches to that model via switch_model.+x op

## What's Missing

❌ **Wildcard completion**:
- `/model *` should list all models (same as no args currently, but explicit)
- `/model g*` should list models starting with 'g' (groq-tool-use-mac, gemma-lan, gemini-flash)
- `/model xyz*` should show "no matches" or similar

## Reference Implementation

**Location:** `/home/no/Desktop/🤖️🪤️🏠️/🥡️🪜️/🪜️-00.00/NNEST-11.01/x0.parent-level-dev-env-04.00/1.TPMOS_c_+rmmp.0102.0028/projects/gem-dev`

**Task:** Investigate how gem-dev handles wildcard/completion patterns, extract the pattern/formula, and adapt it for muchi-pal-agent's model listing.

## What You Need to Do

1. **Read gem-dev code** for wildcard matching implementation
   - Look for `*` pattern matching logic
   - Check how it handles prefix filtering
   - Identify the reusable formula

2. **Extend send_message.c** (ops/send_message.c):
   - Current code: `if (strcmp(buffer, "/model") == 0)` → list all models
   - Add: `if (strncmp(buffer, "/model ", 7) == 0 && strchr(buffer + 7, '*'))` → handle wildcards
   - Extract wildcard pattern (e.g., "g*" from "/model g*")
   - Filter model_list.txt against pattern
   - Output matching models

3. **Implementation details:**
   - Model list format: pipe-delimited (id|provider|api_url|name), skip comments (#)
   - Extract first field (id) for matching
   - Wildcard only supports `*` at end (`prefix*`), not middle or start
   - If no matches, output "No models matching: [pattern]"

4. **Test thoroughly:**
   - `/model *` → all models
   - `/model g*` → gemma-lan, gemini-flash, groq-tool-use-mac, groq-tool-use-mac
   - `/model random*` → random-words
   - `/model xyz*` → no matches message
   - `/model groq*` → groq-tool-use-mac only

5. **Rebuild and verify:**
   - bash button.sh build
   - Test in real terminal

## Files to Modify

- **ops/send_message.c** (line ~583 where `/model` command is parsed)
- Add wildcard matching logic before the `strncmp(buffer, "/model ", 7)` check

## Success Criteria

User can:
- Type `/model *` and see all models
- Type `/model g*` and see only models starting with 'g'
- Type `/model xyz*` and get "no matches" feedback
- All existing `/model` and `/model <id>` behavior still works

## Notes for Sonnet

- This is a small, self-contained task (wildcard matching in C)
- The gem-dev pattern is the reference; we just need to adapt it for this simpler case
- Keep the implementation minimal and readable
- User prefers reusing proven patterns over inventing new ones
- Integration point is send_message.c's command dispatch section
