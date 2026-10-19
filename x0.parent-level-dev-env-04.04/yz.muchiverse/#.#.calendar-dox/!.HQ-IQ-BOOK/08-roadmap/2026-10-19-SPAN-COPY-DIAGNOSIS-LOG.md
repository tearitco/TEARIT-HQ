# 2026-10-19 — Log A: how sure am I about the "mouse diagnostics"?

**Answer up front: I was wrong, and the wrongness was mine, not the
renderer's.** The claim I wrote in the handoff — *"one bug with two entry
points (drag vs. click-range); the range never lands and the copy falls back
to the whole text"* — is **retracted on both halves**, by measurement.

- *"mouse"* — `nb_span_select_test.sh`'s failing steps **S1/S2 are keyboard**,
  not mouse: `send 221 221` (Shift+Right ×2) then `send 3` (Ctrl+C). Its
  header says so. Only `nb_span_drag_test.sh` is mouse.
- *"two entry points, one bug"* — different handlers on different layers
  (`seg_anchor_extend` in the shared draw lib vs. the Button1Motion branch in
  the render). They cannot be assumed to share a cause; I had no evidence.
- *"the range never lands"* — **false in one of the two failing sub-modes**
  (see 3 below: the band *was* painted and the range still did not copy).

What follows is what is actually proven, with the instrument that proves it.

## 1. The measurement that settles the "renderer vs. test" question

`nb_span_select_test.sh` already waits on a **renderer-side copy receipt**
(`kh_focus_debug_log("COPY len=%d text=[%s]")`, render `:10844`) before it
reads the X clipboard — added 2026-10-17 precisely because `xclipget.py`
cannot tell "the answer" from "the last answer". That receipt is **ground
truth about what the renderer copied**, so it discriminates the two theories
that were previously indistinguishable.

8 consecutive runs, reading the **first** COPY line of each run (S2 is the
first copy in the suite, so the first line *is* S2's):

| run | exit | first COPY line of that run |
|---|---|---|
| 1,2,3 | 0 | `COPY len=12 text=[ONE then TWO]` |
| **4** | **1** | **`COPY len=33 text=[go to ONE then TWO then THREE now]`** |
| 5,6,7 | 0 | `COPY len=12 text=[ONE then TWO]` |
| **8** | **1** | **`COPY len=33 text=[go to ONE then TWO then THREE now]`** |

```
FAIL run 4: S1: no highlight band (0)
            S2: clipboard was [go to ONE then TWO then THREE now], want [ONE then TWO]
FAIL run 8: S2: clipboard was [go to ONE then TWO then THREE now], want [ONE then TWO]
```

**The renderer itself logged that it copied the whole label.** So:
it is **not** a stale clipboard read, **not** a test-harness artifact, and
**not** mine (neither suite arms find — `grep -c 'Ctrl+F\|find in page'`
→ `0` in both, and the re-search sits behind `if (g_find_query[0])`).
It is a **product bug in the copy decision**. The `len=33` value is a
coincidence trap: it is *also* what S3/S4/S5/S8 legitimately copy, which is
exactly why the old stale-read theory looked convincing (and why the test's
own 2026-10-17 comment chased it for a while). The receipt is what breaks the
tie.

### Confidence

| claim | confidence | basis |
|---|---|---|
| failing runs' copy is the whole label | **certain** | renderer's own receipt, 2/2 failing runs |
| it is not a stale read / not the harness | **high** | the receipt is the renderer's, not the test's |
| not caused by the find/reparse fix | **high** | both suites never arm find; code is behind `if (g_find_query[0])` |
| the state is `seg_anchor_on(f) == false` at Ctrl+C | **high** | that is the only gate before whole-label copy (`:13006`) |
| **why** it is false | **none** | two sub-modes, one of them unexplained; see below |
| it is the same bug as `nb_span_drag`'s T2 | **none — retracted** | different handler, different layer, no evidence |

## 2. The gate, precisely

```c
khtpm_draw_core.c:1149  seg_cursor_on(e)  = e && g_seg_idx >= 0 &&
                          g_seg_nav == e->nav_index && e->nav_index > 0 &&
                          e->id[0] && strcmp(g_seg_id, e->id) == 0;
khtpm_draw_core.c:1157  seg_anchor_on(e)  = seg_cursor_on(e) &&
                          g_seg_anchor >= 0 && g_seg_anchor != g_seg_idx;
khtpm_core_render.c:13006  Ctrl+C: if (f && seg_anchor_on(f)) copy(range);
                           ... else if (f->label[0]) copy(f->label);   // 13045
```

Anchors are set only in `seg_anchor_extend` (`khtpm_draw_core.c:1658`):

```c
if (fresh || g_seg_anchor < 0 || g_seg_anchor == cur)
    g_seg_anchor = (prev >= 0 && prev < nact) ? prev : cur;
```

and killed at `khtpm_core_render.c` `:3351` (reparse), `:13101` (Esc),
`:13265`/`:13299` (digit nav), `:14981` (click).

## 3. Two sub-modes — and one is already a known, once-fixed bug class

**Mode A — no band at all** (run 4). `seg_anchor_on` false *and* the band
absent: the row drew no highlight because there is no selection. This is
**verbatim** the bug the 2026-10-17 comment at `khtpm_draw_core.c:1643-1657`
describes:

> the index moved, the anchor stayed -1, `seg_anchor_on()` went false, and the
> row drew no band at all. Ctrl+C then took the no-selection path and copied
> the WHOLE label. Measured live: a 205-character copy where a two-character
> range had just been asked for.

That fix replaced `if (fresh) g_seg_anchor = cur;` with the `prev`-aware line
above. **The symptom is back (2/8), so that fix is incomplete — a second path
reaches `seg_anchor_on() == false`.** Finding which one is the job.

**Mode B — band present, copy still whole-label** (run 8: S1 passed!). This is
the mode that kills "the range never lands": the highlight *was* painted, yet
Ctrl+C took the whole-label path. That means the band draw and the copy do not
consult the same state (the draw can paint while `seg_anchor_on(f)` is false
for the *focused* element — e.g. focus drifted off the anchor row without the
band being invalidated), or the state changed between the S1 dump (relay 206 →
forced redraw) and the Ctrl+C. **Unexplained. Do not guess it.**

## 4. The next measurement (cheap, and it is the one that ends this)

The COPY receipt is what turned a "40 % flaky" mystery into a fact. Add the
same kind of one-line receipt **at the decision site**, not in the test:

```c
/* :13006, before the gate */
kh_focus_debug_log("SPANCOPY f=%s fnav=%d seg_nav=%d idx=%d anchor=%d cursor_on=%d anchor_on=%d",
                   f ? f->id : "-", f ? f->nav_index : -1, g_seg_nav, g_seg_idx,
                   g_seg_anchor, f ? seg_cursor_on(f) : 0, f ? seg_anchor_on(f) : 0);
```

Then one failing run names the culprit outright: `anchor=-1` (mode A — a
missing anchor-set path), `anchor==idx` (collapsed — the extend never moved),
`cursor_on=0` (identity drift — `g_seg_nav`/`g_seg_id` vs the focused row),
`anchor_on=1` yet the whole label copied (the code path itself is wrong).
**Predict-then-verify: state which of the four before looking, or the line
teaches nothing.**

## 5. Do not do these (each is a trap already paid for)

1. Do **not** "fix the flaky test instrument". The receipt proves the
   renderer's copy was genuinely the whole label. Re-tuning `clip()` would
   hide a real product bug — and the test's own history shows the last
   instrument bug here was a stale read that *passed* while being wrong.
2. Do **not** conclude "stale clipboard" from the `len=33` value. It is the
   same string several later steps copy on purpose.
3. Do **not** re-apply the copy-receipt patch to `nb_selrun`/`nb_span_drag`:
   already tried, measured worse on both, reverted (handoff §5c).
4. Do **not** treat `nb_span_select` (keyboard) and `nb_span_drag` (mouse) as
   one bug without a second experiment. That inference is the error this log
   exists to correct.

## 6. Measured rates, for the record

| suite | runs | result |
|---|---|---|
| `nb_span_select_test.sh` | 8 (this log) | 6 pass / 2 fail (run 4 = S1+S2, run 8 = S2) |
| `nb_span_select_test.sh` | 6 (earlier today) | 5 pass / 1 fail (S2 only) |
| `nb_span_drag_test.sh` | 6 | 3 pass / 3 fail (T2 ×3, T6 ×2; T6's log shows `CLICKED LINK1` — the deferred click *does* complete sometimes) |

K1 stays red until this is fixed. `nb_span_select` is the sharper target of
the two: its receipt makes it decidable.
