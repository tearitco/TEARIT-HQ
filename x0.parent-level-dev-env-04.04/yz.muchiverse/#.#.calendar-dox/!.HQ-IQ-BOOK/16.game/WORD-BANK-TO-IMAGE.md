# Word bank to image: plan (roadmap G3), 2026-10-13

Status: **W0 built and run 2026-10-13 (section 7); W1 and W2 are plan only.** Owner asks (SAID): run something in python now; aim for an in-house model written as events/pal/ops like the rest of the house; hand-tune words and outputs; use workers where possible; claude and the owner both judge; nothing generated at runtime. Labels: READ = from files or commands run today; MINE = proposal.

## 1. What this machine has (READ, commands run 2026-10-13)
- python `torch 2.9.1` and `diffusers 0.36.0` are installed. `torch.cuda.is_available()` is **False**: no GPU. 8 cores, 29 GB RAM, ~22 GB free.
- I searched the disk for model weights over 100 MB (`*.safetensors`) and found none. So there is no Stable Diffusion model on this machine; one has to be fetched, and its licence checked first.
- Earlier house thinking exists and is the road to "in-house": `#.Z.HUMAN_LLM/3.stage.llm.tomom.../dox/tiny-ai.goal...md` plans a small emoji renderer distilled ("KD") from a big Stable Diffusion XL teacher whose job is to produce prompts and ideal images; `#.ref/menu/palletes/pallette-design.txt` already lists a GENERATION palette with stable-diffusion. Neither is built (not run by me).
- **Offload rule (owner, 2026-10-13): big or GPU work goes over ssh to the other machines (`debil`, the Mac), not on this local dev/test machine.** This machine only does small things: building ops, running W0, judging, contact sheets. Image generation (W1) and any training (W2) run remotely; results come back as files. Heavy local jobs, if ever unavoidable, run under `nice -n 15 ionice -c3`.
- Checked read-only over ssh today: `debil@10.0.0.16` (key login works) has 12 cores, 62 GB RAM, **no `nvidia-smi` (no NVIDIA GPU seen)** and **no python `torch`**. `lfs.master@10.0.0.144` (the Mac) answered `uname -m` = `x86_64` by key login; I did not check its GPU, python or RAM. So neither machine is shown to have a usable GPU yet; what each can run is still to be measured before W1 picks a model. Hosts table: `09-appendix/LAN-HOSTS.md`; logins stay in the owner's note, never in tracked files.

## 2. Three stages (MINE)
| Stage | What | Proof (observable) | Status |
|---|---|---|---|
| W0 | **Banks first, no image model.** A word bank (words for how a thing looks) and a concept-bank entry per part (words + weights + which brixel part), as `.pdl` rows. Prompts are built by a pal/op from the bank. | A prompt file for one part (a car door) built from bank rows by an op; change a weight and the prompt text changes | planned |
| W1 | **Python stand-in drafts images off-line.** A small python script (not a house runtime) calls a free image model on CPU or through a worker, writing PNG + the prompt + the bank rows used into a draft folder. Hand-tuning = editing the bank, not the image. | One concept -> one PNG at 32 px and one at a larger size, with the prompt recorded; I judge, the owner sees a contact sheet; both yes -> the PNG becomes a brixel part's picture | planned, needs a model choice and a licence check |
| W2 | **In-house renderer.** A small model trained on the accepted W1 outputs and their bank rows (distillation, per `tiny-ai.goal`), run as a C op reading bank rows and writing the picture; events/pal drive it. | The op makes the same picture from the same rows with no python and no outside model; a held-out concept (not in training) is drawn and judged | planned, far |

## 3. Rules (from existing house rails)
- A model drafts; it never sets a weight, never promotes a bank row, never approves its own output. Weight changes arrive as candidate EDIT lines checked by `concept_edit_validate` (the same path as physics rows), and a human promotes.
- Every draft appends one marker-ledger line (concept, prompt, model, file, verdict). This is the training set for W2.
- Heavy runs go under `nice -n 15 ionice -c3`. Nothing is generated while a game runs.
- Free models only; Ollama/Gemma calls go to the Mac.

## 4. W0 as built
Files (all under `44.xyz.01.00/&.widgits/concept-bank/`): `data/img/words.pdl` (19 `WORD | id | text | kind | weight` rows; kinds subject, material, colour, angle, style, size, negative; weight 0..1), `data/img/parts.pdl` (4 `PART | brixel | word ids | note` rows for `car_door`, `car_wheel`, `car_windshield`, `person_torso`, ids from `shared/brixels.pdl`), `ops/img_prompt.c`. The op prints `PROMPT`, `NEGATIVE`, `USED` (ids and weights) and `DROPPED` (below the cutoff, default 0.30). It calls no model.
Weights are bank values like any concept-bank weight: they change only by a candidate EDIT that a human promotes (the physics/spoke validator `concept_edit_validate` today covers spoke weights only, so a check for image-word edits is still to be written).

## 5. Not decided
Which image model and its licence; whether CPU generation at 32 px is fast enough or the Mac must do it; the size of the W2 model; where the ledger lives (proposed: beside the draft folder).

## 6. Not verified
No model was loaded and no image generated. The 19 words and 4 parts are a hand-written starter, not tuned; the wording has not been shown to make a good picture. `concept_edit_validate` does not check image words yet. W1/W2 are proposals. debil's and the Mac's image-generation ability is unmeasured.

## 7. Proof for W0 (observed 2026-10-13, fresh build of `img_prompt.c`)
- `img_prompt data/img/words.pdl data/img/parts.pdl car_door` printed: `a car door panel, painted steel, dark red, seen from above, pixel art, plain flat background, crisp outline, 32 pixel sprite` with negative `text, watermark, blurry`.
- Raising `w_dust` from 0.20 to 0.50 changed the `car_wheel` prompt (it gained `worn and dusty`); raising the cutoff to 0.85 cut the door prompt to `a car door panel, seen from above, pixel art, 32 pixel sprite`.
- Negative tests, each exits 1 with its message: weight 1.8, unknown word id in a part, unknown brixel, bad kind, duplicate word id (copies in `data/img/bad/`).
- How to use: run the op as in `data/img/README.md`. To change what a part looks like, edit its word ids in `parts.pdl` or a word's weight in `words.pdl`; rerun and read `PROMPT`.
