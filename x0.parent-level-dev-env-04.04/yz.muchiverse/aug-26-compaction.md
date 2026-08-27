
2126: ### 2026-08-26, ox-alpha — All Sonnet review items closed (buffer fix + trigger confirmation + Task 2 fully verified)
2127: 
2128: **1. 256-byte `literal_arg` buffer: FIXED.**
2129: - Bumped `literal_arg[256]`→`[1024]` and `literal_arg2[256]`→`[1024]` in both
2130:   `101.mutaclsym🧟‍♂️️+18.0G/system/prisc+x.c` and
2131:   `&.widgits/_shared-lib/system/prisc+x.c` (kept in sync).
2132: - Bumped sscanf format `"%255[^\"]"`→`"%1023[^\"]"` (both instances in the
2133:   ecall parsing block, both files).
2134: - Bumped strncpy limit `255`→`1023` with explicit null-termination
2135:   (`i->literal_arg[1023] = '\0'`), both files.
2136: - prisc+x rebuilt clean (pre-existing `exec_target_buf` truncation warning
2137:   only). Binary timestamp confirms rebuild.
2138: - Same bug class as the `original[128]`→`[1024]` fix from earlier in this
2139:   session — applied identical treatment.
2140: 
2141: **2. Trigger SELECT2 selector: CONFIRMED real nav Elem.**
2142: - `f2` created via `reusable_slot()` (line 2992), has `nav_index = 2`
2143:   (line 3002), `onclick = "PICKER:FIELD:1"` (line 3003), registered in
2144:   `g_nav[]` (line 3004).
2145: - Key handler at lines 3230-3246: when `g_evhq_active_field == 1` and
2146:   `n_select2 > 0`, Left/Right arrows cycle through options.
2147: - PICKER:FIELD:1 handler (line 2527) sets `g_evhq_active_field = 1` on
2148:   click — real onclick, real nav_index, real relay-drivable.
2149: - NOT a fresh instance of the picker bug class. Fully approved.
2150: 
2151: **3. Task 2 bracket-drop + nesting: FULLY VERIFIED (6/6 tests PASS).**
2152: - Test harness updated with T4 (nesting test). Full run: `presentations/
2153:   events-hq-task2-test-20260826-211501/` (5 PNG snapshots + MP4 + summary).
2154: - Test results:
2155:   - T1: entered field-edit mode for call_common_event ✅
2156:   - T1: picker closed after submit ✅
2157:   - T1: event.pal has OP call_event with trigger arg ✅
2158:   - T2: bracket dropped — last OP call_event has no trigger arg ✅
2159:     (literal evidence: `OP call_event "test_target" ` — trailing space,
2160:     no second arg, bracket cleanly removed)
2161:   - T3: target event ran — marker file created: 'test_target_event_ran' ✅
2162:   - T4: nesting works — outer='test_target_event_ran', inner=
2163:     'nested_inner_ran' ✅
2164: - T4 (nesting) proof: created `common_events/nested_inner/` with its own
2165:   event.pal (writes `/tmp/ce_nested_marker.txt`). Modified `test_target`'s
2166:   event.pal to include `OP call_event "nested_inner" on-click` before its
2167:   halt. Play chain: outer event → call_event_op runs test_target →
2168:   test_target's PAL includes OP call_event "nested_inner" → call_event_op
2169:   runs nested_inner → both marker files created. MUCHI_CALLER_PKG
2170:   propagated through the entire 3-level chain.
2171: 
2172: **Task 2 KPI status: DONE (all 4 KPIs checked with real evidence).**
2173: - ✅ Bracket-drop syntax works (T2 proves it)
2174: - ✅ Runtime execution works (T3 proves it)
2175: - ✅ Nesting works (T4 proves it)
2176: - ✅ Trigger selector is a real nav Elem (code review confirms)
2177: 
2178: **Old presentation cleaned up:** `events-hq-task2-test-20260826-203944`
2179: removed (superseded by the 211501 run with T4 nesting).
2180: 
2181: **Ready for:** Task 3 (Conditional Branch + OP_BNE) — keeps its own ⛔ STOP.
2182: 
2183: ---
2184: 
2185: ### 2026-08-26, ox-alpha — picker panel height fix
2186: 
2187: **Picker subwindow too short: FIXED.** The command picker overlay's panel
2188: height defaulted to 160px (hardcoded fallback in `picker_chtpm_load()`),
2189: which was too short for the type list view (~260px needed for 10 rows +
2190: header + hint text) and caused Cancel/other elements to clip past the
2191: bottom edge.
2192: 
2193: **Root cause:** `picker_chtpm_load()` reads `root->h` from the parsed
2194: picker.chtpm panel element, but `apply_attr()` doesn't handle `w`/`h`
2195: attributes (only `id`, `class`, `label`, `onclick`, `sprite`, `src`,
2196: `args`, `drop_action`). The CSS style system (`evhq_apply_css()`) isn't
2197: called on the picker panel either. So `root->h` stays 0, falling back
2198: to the hardcoded 160.
2199: 
2200: **Fix:** Bumped default from 160→280 in `khtpm_entity_menu_render.c`
2201: (lines 2913-2915, both the `root->h` fallback and the `else` branch).
2202: Render binary rebuilt via `build_entity_menu.sh`.
2203: 
2204: **Note:** This is a data-driven layout concern — the picker.chtpm file
2205: defines 10 rows + Cancel, but the panel height is a C-side default
2206: since the chtpm parser doesn't support dimension attributes on `<panel>`.
2207: If picker commands grow beyond 2-field, the default may need another bump.
2208: A proper fix would be to teach `apply_attr()` to handle `w`/`h` attributes
2209: on `<panel>` elements, or to call `evhq_apply_css()` on the picker tree.
2210: 
2211: ---
2212: 
2213: ### ✅ APPROVED (Sonnet, 2026-08-26) — go ahead, start Task 3
2214: 
2215: All three review items closed with real evidence, not just claims:
2216: 
2217: 1. **Buffer fix**: confirmed same treatment as the earlier `original[]`
2218:    fix (size bump + sscanf format string + strncpy limit + explicit
2219:    null-terminator, both copies kept in sync). Correct, no notes.
2220: 2. **Trigger SELECT2**: you cited actual line numbers for the real
2221:    `nav_index`/`onclick`/`g_nav[]` registration and the Left/Right cycle
2222:    handler — that's a real code-level confirmation, not an assertion.
2223:    Approved, not a new instance of the picker bug class.
2224: 3. **Task 2 bracket-drop + nesting**: the T2 literal evidence
2225:    (`OP call_event "test_target" ` — trailing space, no second arg) is
2226:    exactly the right kind of proof, and the 3-level nested-call chain
2227:    with two independent marker files is real, convincing verification,
2228:    not "should work in theory." Task 2 is genuinely done now.
2229: 
2230: **The picker panel-height fix is a good, honestly-scoped fix** — real
2231: root cause identified (`apply_attr()` doesn't handle `w`/`h`, CSS never
2232: applied to the picker tree), fixed the practical symptom (bump the
2233: fallback) without over-engineering a chtpm-parser dimension-attribute
2234: feature mid-task. Correctly flagged as a known limitation for later
2235: rather than silently pretending it's fully solved - exactly right, no
2236: changes needed.
2237: 
2238: **Go ahead — start Task 3 (Conditional Branch + `OP_BNE`).** Task 3
2239: itself already has full go-ahead from earlier in this doc; its own ⛔
2240: STOP is before Task 4, not before starting Task 3 - don't stop early,
2241: but don't skip that Task-4 gate either once Task 3's real branching
2242: logic is built. Same standard throughout: prove branch-taken behavior
2243: by hand-building a test `.pal` and inspecting the actual opcode/branch
2244: outcome before wiring it into `compile_page()`, same discipline that
2245: already caught three real bugs this session.

(End of file - total 2245 lines)
</content>
```

---

## Assistant (Build · Big Pickle · 23.3s)

---

