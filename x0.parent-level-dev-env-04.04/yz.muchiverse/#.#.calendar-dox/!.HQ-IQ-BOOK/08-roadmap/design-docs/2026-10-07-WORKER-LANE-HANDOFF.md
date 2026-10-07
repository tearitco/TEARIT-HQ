# Handoff: worker-lane findings from opencode browser work (2026-10-07)

For the `opencode-fix` lane (owns `ops/`, `tests/worker_*`, FETCH protocol,
`nb_js_worker` sides). All observed live against Blockly + fixtures; none
of these were changed on the opencode side per the lane split.

## 1. Worker RENDER LINK rows shaped `LINK|<url> <label>` (space, not pipe)

Live click test proved the consequence: the projector read the whole
string as the URL and submitted it as a YouTube search. opencode-side
defense is in (`9665531b0`: whitespace URLs never navigate, label shown
as text). The emitter-side fix (emit `LINK|<url>|<label>`) belongs here.

## 2. Worker LINK text concatenation drops separators (`1History`)

TOC anchors arrive as `1History` instead of `1 History`. The static
extractor inserts spaces at inner-tag boundaries (`0f61a9c49`); the
worker DOM walk apparently does not. Same class of fix in `dom_walk_render`.

## 3. Worker emits zero IMG rows on `file://` pages

Static MEDIA fallback covers it manager-side (`9ca69d596`), so tiles
still render, but JS-driven file pages depend on the worker path.
Suspect: `img_get_src` resolution against `file://` bases.

## 4. Worker output is nondeterministic run-to-run on the same URL

Same Blockly URL yielded 8, 3, then 0 IMG rows across loads minutes
apart. Makes grid/media verification flaky; the opencode harness pins
behavior with file fixtures + normalization instead. Worth a look when
the lane has time: likely JS-timing/LOAD race in slice evaluation.

## 5. Untouched by agreement

`nb_js_worker.c`, `nb_host.h`, FETCH protocol, `tests/worker_*`,
`network-browser-hq.xhtpm` contract (no segment rows added from our
side). Manager handshake file shared as agreed.

## 6. QuickJS refcount rule (read this before touching JSValues)

Verified in this vendored tree (`set_value`, `add_fast_array_element`):
**`JS_SetProperty*` CONSUMES the value — do NOT `JS_FreeValue` after a
set.** `JS_GetProperty*` returns a new reference — MUST free. Getting
this backwards is a premature-free UAF (ASan-caught live 2026-10-07:
worker died on `addColorStop`). Our added canvas/WebGL code now follows
consume semantics throughout; pre-existing `Set+Free` instances
elsewhere in the file are latent use-after-frees for this lane to audit.
