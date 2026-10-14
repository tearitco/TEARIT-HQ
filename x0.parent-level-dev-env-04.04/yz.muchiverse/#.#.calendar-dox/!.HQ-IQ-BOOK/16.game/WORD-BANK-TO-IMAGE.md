# Word bank to image: plan (roadmap G3), 2026-10-13

Status: **PLAN only. Nothing here is built or run.** Owner asks (SAID): run something in python now; aim for an in-house model written as events/pal/ops like the rest of the house; hand-tune words and outputs; use workers where possible; claude and the owner both judge; nothing generated at runtime. Labels: READ = from files or commands run today; MINE = proposal.

## 1. What this machine has (READ, commands run 2026-10-13)
- python `torch 2.9.1` and `diffusers 0.36.0` are installed. `torch.cuda.is_available()` is **False**: no GPU. 8 cores, 29 GB RAM, ~22 GB free.
- I searched the disk for model weights over 100 MB (`*.safetensors`) and found none. So there is no Stable Diffusion model on this machine; one has to be fetched, and its licence checked first.
- Earlier house thinking exists and is the road to "in-house": `#.Z.HUMAN_LLM/3.stage.llm.tomom.../dox/tiny-ai.goal...md` plans a small emoji renderer distilled ("KD") from a big Stable Diffusion XL teacher whose job is to produce prompts and ideal images; `#.ref/menu/palletes/pallette-design.txt` already lists a GENERATION palette with stable-diffusion. Neither is built (not run by me).
- The Mac (10.0.0.144) is where LAN model calls go; I have not checked whether it can run image generation.

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

## 4. Next step for W0 (cheap, no model)
Design the rows: `WORD | id | text | kind`, `CONCEPT_IMG | brixel | part | words | weights`. Build the prompt op. This also tells us which parts the first W1 trial needs.

## 5. Not decided
Which image model and its licence; whether CPU generation at 32 px is fast enough or the Mac must do it; the size of the W2 model; where the ledger lives (proposed: beside the draft folder).

## 6. Not verified
No model was loaded, no image generated, no bank rows written. The stage list is a proposal.
