#!/usr/bin/env python3
"""img_draft.py - W1 stand-in (BOOK 16.game/WORD-BANK-TO-IMAGE.md). Runs on debil/Mac, NOT on the dev machine. Off-line drafting only; the game never calls it.
Usage: img_draft.py --prompt-file P.txt (output of img_prompt: PROMPT:/NEGATIVE: lines) --part car_door --model nota-ai/bk-sdm-tiny --out DIR [--steps 20 --size 512 --seed 1 --px 32]
Writes DIR/<part>_s<seed>.png (full size), DIR/<part>_s<seed>_<px>px.png (nearest-neighbour, 16-colour palette) and appends one ledger line to DIR/ledger.txt:
LEDGER | part | model | seed | steps | size | seconds | file | prompt   (verdict is added later by the judges, never by this script)."""
import argparse, os, time
ap = argparse.ArgumentParser()
ap.add_argument("--prompt-file", required=True); ap.add_argument("--part", required=True); ap.add_argument("--model", required=True); ap.add_argument("--out", required=True)
ap.add_argument("--steps", type=int, default=20); ap.add_argument("--size", type=int, default=512); ap.add_argument("--seed", type=int, default=1); ap.add_argument("--px", type=int, default=32); ap.add_argument("--guidance", type=float, default=7.5)
a = ap.parse_args()
pos = neg = ""
for ln in open(a.prompt_file):
    if ln.startswith("PROMPT:"): pos = ln[7:].strip()
    elif ln.startswith("NEGATIVE:"): neg = ln[9:].strip()
if not pos: raise SystemExit("no PROMPT: line in " + a.prompt_file)
import torch
from diffusers import StableDiffusionPipeline
torch.set_num_threads(int(os.environ.get("IMG_THREADS", "6")))
os.makedirs(a.out, exist_ok=True)
pipe = StableDiffusionPipeline.from_pretrained(a.model, torch_dtype=torch.float32, safety_checker=None)
pipe = pipe.to("cpu")
t0 = time.time()
img = pipe(pos, negative_prompt=neg or None, num_inference_steps=a.steps, guidance_scale=a.guidance, height=a.size, width=a.size, generator=torch.Generator("cpu").manual_seed(a.seed)).images[0]
secs = time.time() - t0
base = os.path.join(a.out, "%s_s%d" % (a.part, a.seed))
img.save(base + ".png")
small = img.resize((a.px, a.px), resample=0).quantize(colors=16).convert("RGB")
small.save("%s_%dpx.png" % (base, a.px))
with open(os.path.join(a.out, "ledger.txt"), "a") as f:
    f.write("LEDGER | %s | %s | %d | %d | %d | %.1f | %s | %s\n" % (a.part, a.model, a.seed, a.steps, a.size, secs, os.path.basename(base) + ".png", pos))
print("done %.1fs %s" % (secs, base + ".png"))
