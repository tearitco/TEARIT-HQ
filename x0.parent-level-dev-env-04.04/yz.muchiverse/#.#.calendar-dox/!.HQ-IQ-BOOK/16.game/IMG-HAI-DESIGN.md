# img-hai: a text-to-image window for brixels (design, 2026-10-13)

Owner (SAID, 2026-10-13): "a new h-ai for a text 2 image x11-hq gui interface for generating images / brixels". Status: **design only, nothing built.** Parent plan: `WORD-BANK-TO-IMAGE.md` (W0 prompt op and W1 drafts exist; this window is the face for them). Labels: READ = from files; MINE = proposal.

## 1. What it is
A new app `&.hq-apps/img-hai/`, built like its siblings `chat-hai` and `co-lab-hai` (READ: static `.xhtpm` template, a small C manager that writes `state/ui.txt`, `button.sh` that starts the shared `khtpm_core_render.+x`, `ops/`, a design doc). One window to take a brixel part from words to a judged image:
1. **Pick a part** (`car_door` ... from `data/img/parts.pdl`); the window shows its words and weights and the prompt `img_prompt` builds.
2. **Pick a host and model** (debil, later the Mac; model from a short list) and press **Draft**. The manager runs the python stand-in on that host over ssh and shows status (queued, running, seconds, done/failed). It never blocks the window.
3. **Look at the results** as a contact sheet of the part's drafts, with their ledger lines.
4. **Judge:** buttons yes / no for the owner and for claude (two separate rows, because both must say yes). Verdicts append to `verdicts.txt` in the draft folder.
5. **Accept:** only when both verdicts are yes does an Accept button enable. Accept writes a *candidate* concept row linking the image to the brixel part for the normal human promotion path; it does not edit the bank directly.
6. **Edit words:** changing a word weight or a part's word list produces a candidate EDIT line (checked, then a human promotes), not a direct write. A model never sets a weight.

## 2. Rules it must keep
- Big/GPU work runs on debil or the Mac over ssh (owner's offload rule), never on the dev machine; local work is only prompt-building, copying a PNG back, and drawing.
- One ledger line per draft and per verdict, append-only, size-growth markers rather than mtime for change detection (house rule). No extra logging beyond that.
- The ssh job runs from a forked child with a watchdog and no `popen`-to-EOF (the known freeze in the house's prisc custom-op path); the manager polls a marker.
- No credentials in tracked files. Free models only; model licence recorded in the ledger/model list.
- Nothing is generated while a game runs; this window is the only caller.

## 3. Slices (each ends with a screenshot + a state file the owner can read)
| # | Slice | Proof | Status |
|---|---|---|---|
| H1 | Window opens; read-only: lists parts and shows the built prompt and the existing ledger and verdicts | window screenshot via `dump_frame_png_op`; `state/ui.txt` shows the 4 parts and the 4 existing car_door drafts | planned |
| H2 | Draw the contact sheet inside the window (needs a way to show PNGs, see section 4) | the 4 car_door drafts visible in the window | planned |
| H3 | **Draft** button: ssh job to debil with status | a new ledger line appears and the new image shows, while the window stays responsive during the ~2 min run | planned |
| H4 | Verdict buttons (owner row, claude row) and Accept gated on both | a verdict line per click; Accept stays disabled with one yes; with two yes it writes a candidate row | planned |
| H5 | Second host (Mac) and a model list | same part drafted on both, two ledger lines with host and seconds | planned |
| H6 | Brixel view: show the accepted part in an assembly | the accepted door on the car assembly | planned |

## 4. Open questions
1. **Name and place:** `img-hai` under the `14.h-ai` group like `chat-hai` (MINE: yes), or a different name?
2. **Showing PNGs inside a khtpm window:** the shared renderer's swatch-grid / sprite paths exist (READ in the khtpm standards), but I have not confirmed an image element that loads an arbitrary PNG. If there is none, H2 needs the existing sprite path or a small shared addition (which must follow the khtpm rule: reuse, never a new per-app branch). To check before H2.
3. **Who owns the model list** (which free models, licences): proposal is a `models.pdl` beside the words, reviewed by you.
4. Should Accept also scale the image to the part's 32 px cell, or only store the large image and let the detail-level op (G2) scale it? (MINE: store both, the op decides.)

## 5. Not verified
No window exists; I have not checked how a PNG is shown in a khtpm window; ssh job handling is described, not built.
