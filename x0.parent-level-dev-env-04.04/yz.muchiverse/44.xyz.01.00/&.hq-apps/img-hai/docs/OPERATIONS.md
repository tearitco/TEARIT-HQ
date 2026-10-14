# Operating img-hai: machines, setup, repair

## 1. The machines (checked 2026-10-13, read-only unless noted)
| Machine | ssh | CPU / RAM | GPU | python | status |
|---|---|---|---|---|---|
| dev machine (this one) | - | 8 cores, 29 GB | none; torch CPU only | system | **does not draw** (owner rule: GPU and big work go elsewhere); runs the window, ops, tests |
| debil | `debil@10.0.0.16` (key login) | Ryzen 5 5500U, 12 threads, 62 GB, 62 GB free | integrated AMD only, no CUDA | 3.11.2 | draws; venv in `~/imggen/venv`; ~3 GB model cache |
| Mac | `lfs.master@10.0.0.144` (key login) | Intel i7-8850H, 12 threads, 32 GB | Radeon Pro 560X + UHD 630, no CUDA | 3.9.6 | draws; venv in `~/imggen/venv`; ~2 GB model cache |
Neither remote machine has a CUDA GPU, so every draw is CPU: about 2 minutes per 512 px image at 20 steps. Hosts table of the whole house: book `09-appendix/LAN-HOSTS.md`. Logins live only in the owner's note (path in memory `reference-lan-hosts`); never copy them into a tracked file.

## 2. Rebuilding a remote environment (exact commands that worked)
Replace `<target>` and run each through `ssh -o BatchMode=yes <target> '...'`. Run the installs in the background with `nohup nice -n 15 ... > install.log 2>&1 &` and poll `install.log` (they take minutes and download ~0.3-2 GB).
**Linux (debil).** The system python has no `ensurepip`, so bootstrap pip by hand (no sudo needed):
```
mkdir -p ~/imggen && cd ~/imggen
python3 -m venv --without-pip venv
curl -s https://bootstrap.pypa.io/get-pip.py -o get-pip.py && venv/bin/python get-pip.py -q
venv/bin/pip install torch --index-url https://download.pytorch.org/whl/cpu
venv/bin/pip install diffusers transformers accelerate safetensors pillow
```
Versions that worked: torch 2.14.1+cpu, diffusers 0.41.0, transformers 5.19.0, accelerate 1.15.0, numpy 2.4.6, pillow 12.3.0.
**macOS Intel (Mac).** Newest torch wheels stop at 2.2.2 for Intel macOS, and diffusers 0.36+ needs `torch.xpu` (newer torch), which gave `AttributeError: module 'torch' has no attribute 'xpu'`. Pin old versions:
```
mkdir -p ~/imggen && cd ~/imggen && python3 -m venv venv
venv/bin/pip install --upgrade pip
venv/bin/pip install "torch==2.2.2" "numpy<2" diffusers transformers accelerate safetensors pillow
venv/bin/pip install "diffusers==0.27.2" "transformers==4.40.2" "huggingface_hub==0.23.5" "accelerate==0.30.1" "numpy<2"
```
Versions that worked: torch 2.2.2, diffusers 0.27.2, transformers 4.40.2, huggingface-hub 0.23.5, accelerate 0.30.1, numpy 1.26.4, pillow 11.3.0.
**Then, on either:** copy the draft script and check it loads:
```
scp '&.widgits/concept-bank/data/img/draft/img_draft.py' <target>:imggen/
ssh <target> 'cd imggen && venv/bin/python -c "import torch, diffusers; print(torch.__version__, diffusers.__version__)"'
```
The first real draft downloads the model (`~/.cache/huggingface`); it is slow once. **Always re-copy `img_draft.py` to every host after editing it** (an old copy on a host caused a lost draft once: the new job op expected the new file names).

## 3. Everyday operations
| Task | Command |
|---|---|
| open / close the window | `sh '&.hq-apps/img-hai/button.sh' run` / `stop` |
| rebuild the app programs | `bash '&.hq-apps/img-hai/build.sh'` |
| rebuild the word-bank programs | `bash '&.widgits/concept-bank/ops/build_img_ops.sh'` |
| what is the window doing? | `cat '&.hq-apps/img-hai/state/ui.txt'`, `tail '&.hq-apps/img-hai/state/job.txt'` |
| is a draft running on a host? | `ssh <target> 'ps -eo pid,etimes,args \| grep img_draft'` |
| watch a draft's output | `state/job_<id>.out` (present only while/after a failed job) and on the host `~/imggen/jobs/` |
| make a contact sheet | `'&.hq-apps/img-hai/ops/+x/img_sheet.+x' out.png 4 256 <files>` |
| take a screenshot | `relay_drive.+x shot $(relay_drive.+x findwin 1060 900) out.png` |

## 4. Disk and backups
- **Pictures are not in git** (`*/img/` is ignored in `draft/`): 12 drafts ~ 6 MB of PNG. The *ledger* (prompt, model, seed, steps, seconds) is in git, which lets a picture be re-made with the same model and seed: two reruns matched the originals within 0.6-0.9 of 255 (mean pixel difference; the originals were 256 px crops), but CPU float32 output is **not guaranteed identical** across machines and library versions. To keep a picture that matters, copy it out (a tarball of `data/img/draft/*/img`, or `git add -f` one accepted picture, which is the owner's call).
- Remote scratch: `~/imggen/jobs/` on each host keeps the prompt files and a copy of every output (a few MB per day). Safe to delete when the pictures are safe locally: `ssh <target> 'rm -rf ~/imggen/jobs/*'`.
- Model caches: debil 3.0 GB, Mac 2.0 GB. Do not delete between runs unless you want a re-download.
- Local `state/thumbs/` grows (old thumbs are never deleted): `rm -rf '&.hq-apps/img-hai/state/thumbs'` is safe; the window rebuilds them.
- The tile list holds **256** images (each draft makes two: the picture and its 32 px copy). Past that, extra images are not shown (see `HANDOFF.md` open items).

## 5. When something goes wrong
| Symptom | Likely cause | Fix |
|---|---|---|
| window does not open | renderer or manager not built; old window left | `bash build.sh`; `sh button.sh stop` then `run`; read `audit/img-hai.log` |
| click does nothing | two-step click; or the words field is armed | click again / press Esc |
| Draft says `failed: cannot reach host` | ssh down, key missing, wrong ip | `ssh -o BatchMode=yes <target> true` |
| `failed: remote draft (exit 1)` | python error on the host | read `state/job_<id>.out` (end of file has the traceback). Known: Mac `torch has no attribute xpu` = diffusers too new (section 2); `No module named ...` = venv missing/broken |
| `failed: copying the image back` | the remote file is not where the job expects (old `img_draft.py` naming) or scp failed | re-copy `img_draft.py` to the host (section 2); check `~/imggen/jobs/out/` |
| `<host> is busy with a draft` | one draft per host | wait for `done:` |
| a draft finished on the host but not in the window | job op was killed (window closed) | fetch by hand: `scp <target>:imggen/jobs/out/<id>*.png <draft dir>/img/` and append the LEDGER line from `~/imggen/jobs/out/ledger.txt` |
| pictures are noise / wrong thing | small models + weak words | `HANDOFF.md` section 6; tune the bank (`EXTENDING.md` D) |
| thumbnails blue instead of red | someone removed the red/blue swap | restore it in `make_thumb()` (renderer paints PNG sprites as BGRA) |
| `no frame dump after key p` (tests) | a field is armed, or the window is not responding | Esc; re-run |
| the Mac is very slow / hot | CPU-only drafting; it was also running other jobs | one draft at a time per host is enforced; check `top` on the host |
| tile palette missing new pictures | scan not run | press `scan` |

## 6. Safety checklist before running a big batch
Enough free disk on the hosts and here; both hosts reachable; nobody else using the Mac for something heavy; a batch of N jobs takes about N/2 x 2 minutes with two hosts; you can stop the waiting jobs with `Stop batch` but not a running remote draft (it finishes, ~2 min).
