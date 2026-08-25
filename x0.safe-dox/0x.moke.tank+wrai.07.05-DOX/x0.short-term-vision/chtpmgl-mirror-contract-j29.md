# CHTPMGL Mirror Contract J29

Date: 2026-06-29
Status: Working architecture note before deeper `chtpmgl-wraith` implementation.

==================================================
1. CORE RULE
==================================================

ASCII must mirror semantic UI truth, not GL pixels.

Do not build auditability by trying to approximate:

- colors
- gradients
- images as pictures
- video frames
- animation frames
- shader effects

Build auditability from the same interactive/state model that drives GL.

==================================================
2. SHARED TRUTH LAYER
==================================================

`chtpmgl-wraith` should resolve rich UI into a shared semantic layer containing at least:

- object id
- role
- label
- value/state
- nav index
- action
- row/group ownership
- selected/focused state
- relevant asset reference

This layer should be the truth that both surfaces read:

- GL surface: rich presentation
- ASCII surface: audit/debug projection

==================================================
3. WHAT ASCII MUST PRESERVE
==================================================

ASCII mirror must preserve:

- nav order
- focus / selected item
- action labels
- structural grouping
- row ownership
- scroll and thumb state
- important values
- any state needed for agents or users to make decisions

==================================================
4. WHAT MAY DIFFER
==================================================

ASCII does not need pixel parity.

It may simplify:

- exact spacing
- color treatment
- image rendering
- animation visuals
- decorative-only layers

But simplification is not allowed to hide behaviorally meaningful state.

==================================================
5. OBJECT CLASSES
==================================================

Use this rule:

1. interactive
   - must mirror fully in ASCII
2. informational
   - must mirror as readable text/state
3. decorative
   - may mirror as a token or be omitted

If a GL object affects:

- user choice
- agent control
- system state understanding

it is not decorative.

==================================================
6. MEDIA DEGRADATION RULE
==================================================

First-pass rule:

Media should degrade into audit tokens, for example:

- `Theme: amber-dark`
- `BG: image wallpaper_01`
- `Preview: image selected`
- `Audio: track_02 playing`
- `Video: intro_loop paused`
- `3D: rotating preview active`

The asset does not need to be drawn in ASCII, but its identity and state should be visible.

Important future rule:

This token degradation is the minimum contract, not the ceiling.

Because image/video paths already require RGB-style decoded frame data for GL presentation, future ASCII audit surfaces may also consume reduced media projections such as:

- palette/color-block previews
- brightness-ramp ASCII previews
- coarse frame-stream projections for video

So the long-term rule is:

- semantic token/state must exist immediately
- richer RGB-to-ASCII projection may be added later
- both should derive from shared media truth rather than separate one-off render paths

==================================================
7. ROW / NAV CONTRACT
==================================================

Row intent is part of the semantic truth.

- same-row grouped controls should stay grouped
- per-item content rows should stay one item per row unless intentionally grouped
- scroll utility strips are valid shared-row patterns

Standard long-list strip:

- `^_UP`
- `v_DOWN`
- `Thumb:[#-------]start-end/total`

on one row, with content rows below.

==================================================
8. WHY THIS MATTERS FOR WRAITH
==================================================

Without this rule, GL and ASCII will drift into two different UIs:

- one pretty but hard to audit
- one textual but no longer trustworthy

That would break:

- frame-debug usefulness
- agent navigation confidence
- reproducible bug reports
- no-screenshot auditing

So the mirror contract should be treated as a required architecture rule for `chtpmgl-wraith`.
