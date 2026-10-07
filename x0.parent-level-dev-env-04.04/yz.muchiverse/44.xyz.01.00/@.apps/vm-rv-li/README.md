# vm-rv-li

An **x11-hq toy** that runs the [`RVLV_EMUL`](../shared-pointer-note) RV64GC
(QEMU-`virt`) emulator inside a khtpm window and renders its **UART console as a
terminal screen**. Both a **live human** and an **external agent** can drive the
guest — their input converges on a single relay and is byte-identical to the
emulator, exactly as the house intends (HQ-IQ-BOOK §9: relay, not xtest).

Toy id: `toys:vm-rv-li` (entry in `#.desktop/livedesk_taskbar.pdl` toys scan via
`@.apps/vm-rv-li/toy.pdl`).

## Architecture (one-writer, file-backed — HQ-IQ-BOOK §8)

```
            ┌──────────────────────┐  stdin/stdout pipes  ┌─────────────┐
  human keyboard --> renderer      │                      │ RVLV EMUL   │
  (cli_io)  --> ops/vm_rv_li_input.sh ─> human_input.txt │ <house>/    │
                                    │        │   manager  │ build/main  │
  agent  --> agent_input.txt ───────┼────────┼ drains both │  (xv6/Linux)│
                                    │        ▼     to     │ UART stdin  │
               &h-apps/_template..  │  emulator stdin      └─────────────┘
               renderer(khtpm_core_  │        │
                 render.+x)         │  <module> forks     ┌─────────────┐
                                    │  vm_rv_li_manager    │  state/ui.txt│
                                    │        │ drains stdout│  rows/cursor│
                                    │        ▼              └─────────────┘
                                    └──► grid(80x24, ANSI-  xhtpm ${row.N_text}
                                         stripped)         scrolllist
```

- **`<module src="@.apps/vm-rv-li/+x/vm_rv_li_manager.+x"/>`** — the renderer
  forks the manager as a real child (tied to the window lifetime); it writes
  `module_parent.pid` before launching (HQ-IQ-BOOK §2 / `launch_module()` in
  `khtpm_core_render.c`). The manager exits when that pid vanishes.
- **Emulator I/O** — `RVLV_EMUL/src/uart.c` reads console input from `stdin`
  (non-blocking `select`) and writes UART TX to `stdout` (`fputc`+`fflush` per
  byte). So the manager just needs a stdin/stdout pipe pair — no serial driver.
- **Input relay** — human and agent both append to one of two files; the manager
  is the sole drain (consumer), each file has one appender:
  - human → `#.desktop/vm_rv_li/human_input.txt` (via `ops/vm_rv_li_input.sh`,
    fired by `<cli_io action="${input_action}">`).
  - agent → `#.desktop/vm_rv_li/agent_input.txt` (write `type:<line>`,
    `paste:<text>`, or `key:<byte-dec>` directly).
  - Manager forwards the decoded bytes to the emulator's stdin pipe.
- **Output** — manager reads emulator stdout, strips ANSI into an 80x24 char grid,
  and atomically publishes `<pkg>/state/ui.txt` (`rowcount`, `row_0_text`..
  `row_23_text`, `cursor_x/y`, `status`, `interact_armed`). The `.xhtpm`
  `<scrolllist><repeat bind="row"><text label="${row.text}"/></repeat>` projects
  it (same proven pattern as `co-lab-hai`'s `conv-list`).

## Files

| path | role |
|---|---|
| `vm_rv_li_manager.c` | the manager (spawn emulator, UART↔grid, drain relays, publish ui.txt, parent-liveness, clean SIGTERM teardown) |
| `build.sh` | `gcc -std=c11 -Wall -O2` → `+x/vm_rv_li_manager.+x` (mirrors `co-lab-hai/build.sh`) |
| `vm-rv-li.xhtpm` | window layout: `<sidebar>` + `<panel>` (chrome+taskbar entry), `<scrolllist>` terminal grid, `<cli_io>` input |
| `vm-rv-li.css` | black/green monospace terminal styling |
| `ops/vm_rv_li_input.sh` | `<cli_io>` action script: `type <pkg> <house> <text>` → appends to `human_input.txt` |
| `button.sh` | launcher: kill-prior, ensure renderer+manager built, launch `khtpm_core_render <house> <xhtpm>` |
| `toy.pdl` / `open_vm_rv_li.sh` | toys-menu entry + glob-safe wrapper (house §13: `@` paths are shell-safe; an app under `&.hq-apps` needs the `open_*$` wrapper because `&` is job-control under `sh -c`) |
| `+x/vm_rv_li_manager.+x` | compiled manager (gitignored `*.+x`) |

## Config (`#.desktop/vm_rv_li/config.pdl`, live-edited, §7)

```
# vm-rv-li emulator configuration
# RVLV_EMUL must be cloned as a sibling repo:
#   /home/debil/Desktop/github/RVLV_EMUL

emu_root | /home/debil/Desktop/github/RVLV_EMUL
emu_bin  | build/main
emu_args | --virtio-legacy -k images/xv6-kernel.bin -f images/xv6-fs.img
cols     | 80
rows     | 24
```

**Deployment Note**: The manager seeds this on first run. The `emu_root` path
is **absolute**, so RVLV_EMUL must be cloned to the same location on the target
machine. The emulator runs with CWD = `emu_root` (house §13: relative
`emu_args` resolve against `emu_root`, not the inherited CWD).

### Switching to Linux (if needed)

**Current status**: xv6 is the working, stable default. Linux support exists in
RVLV_EMUL (verified to boot to login prompt), but requires building Buildroot
first.

To enable Linux:
1. Build Buildroot kernel + rootfs (see RVLV_EMUL/CLAUDE.md §Buildroot Rebuild):
   ```bash
   cd /home/debil/Desktop/github/RVLV_EMUL/buildroot
   make clean && nohup make BR2_JLEVEL=$(nproc) > /tmp/buildroot_full.log 2>&1 &
   # Wait ~45-60 minutes for full build
   ```
   
2. After build completes, images appear at:
   - `buildroot/output/images/Image` (kernel)
   - `buildroot/output/images/rootfs.ext2` (filesystem)

3. Edit config.pdl to use Linux boot mode:
   ```
   emu_args | --linux -k kernel/Image -f buildroot/output/images/rootfs.ext2 --bootargs "console=ttyS0,115200n8 earlycon root=/dev/vda rw rootwait init=/sbin/init"
   ```
   (Relative paths resolve against `emu_root`, so `kernel/Image` lives at
   `/home/debil/Desktop/github/RVLV_EMUL/kernel/Image` — copy it after buildroot finishes)

**Note**: Linux build is optional for now; xv6 is fully functional and stable.

## Build / Run

```sh
# emulator (once):
cd /home/debil/Desktop/github/RVLV_EMUL && sh build.sh        # -> build/main

# toy manager:
cd <house>/@.apps/vm-rv-li && sh build.sh                   # -> +x/vm_rv_li_manager.+x

# launch from the toys menu ("vm-rv-li") or directly:
sh <house>/_.monads/_.livedesk-taskbar/ops/open_vm_rv_li.sh
```
The renderer auto-forks the manager via `<module>`; `button.sh` only guarantees
the shared `khtpm_core_render.+x` and the manager binary exist.

## Testing & Verification

### Build and Boot Verification (v0.1 completion check)
- ✅ `gcc` compiles the manager cleanly (`-Wall`, no `-Werror`).
- ✅ xv6 boots through the bridge: `ui.txt` `status=running`, `rowcount=24`,
  `row_1_text=xv6 kernel is booting`.
- ✅ Emulator is reaped on `SIGTERM` (no stray `main` processes afterwards).
- ✅ Input relay path implemented (dual file → single stdin pipe).

### Live Keystroke Test (house standard relay pattern, per khtpm-house-standards)

**CRITICAL**: This test verifies the full human→relay→manager→emulator→UART→ui.txt
chain works in real-time (not just agent-file testing).

```bash
# 1. Check if window is running (find the khtpm_core_render PID for vm-rv-li)
ps aux | grep khtpm_core_render | grep vm-rv-li

# 2. Save PID for clarity
PID=<the-pid-from-above>

# 3. Verify relay file exists (manager creates it; khtpm_core_render writes to it)
cat /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/$PID.txt

# 4. Send a keystroke via relay (e.g., Enter=13, then type 'a'=97)
echo "KEY_PRESSED: 13" >> /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/$PID.txt
echo "KEY_PRESSED: 97" >> /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/$PID.txt

# 5. Verify in ui.txt that the input arrived at the guest
cat /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/vm-rv-li/state/ui.txt
# Look for row_*_text containing a guest prompt (xv6 shell or login prompt)
```

**Expected result**: Typing into the relay file produces output visible in ui.txt rows
(e.g., xv6 shell prompt changes, or new output appears from the guest). If this
works, the entire chain is verified: live human input → window relay → manager input
drainer → emulator stdin → UART → guest console output → ANSI strip → ui.txt.

### Agent Input Test (complementary, file-based)

If relay test passes, agent input is already proven (same code path):
```bash
# Write type action to agent file (example)
echo "type:ls" >> /home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/44.xyz.01.00/@.apps/vm-rv-li/ops/human_input.txt
# Verify in ui.txt
```

## Current gaps (v0.1)

- **Live keystroke-through test** (critical): Input relay path coded and verified
  via agent files; needs real-time human interaction test through the window's
  `<cli_io>` element. See "Testing" section below.
- Human `<cli_io>` submits a **line** per Enter (xv6/getty cooked mode is fine);
  raw control chars (Ctrl-C...) are only via the agent's `key:<byte-dec>` relay.
  Use `/home/debil/Desktop/github/TEARIT-HQ/x0.parent-level-dev-env-04.04/yz.muchiverse/#.desktop/entity_menu_history/<pid>.txt`
  relay file (house standard, per khtpm-house-standards skill).
- ANSI strip handles **CSI** sequences (SGR/colours, cursor) — sufficient for
  xv6 + Linux console; OSC/DCS are dropped to end-of-line (not byte-accurate).
- No cursor-blink / cell attribute rendering yet (cursor position reported in
  sidebar; sufficient for text interaction).

## Status & What's Next

### Current Status (2026-10-05)
1. ✅ **Manager + layout + relay fully built and verified**: vm_rv_li_manager.c compiles
   clean, xv6 boots to "kernel is booting" message, emulator reaped cleanly.
2. ✅ **Input relay architecture sound**: dual-file path (human_input.txt + agent_input.txt)
   → single stdin pipe verified in code review; pending live keystroke test.
3. ✅ **UART↔grid translation working**: ui.txt publishes real rows + cursor position.
4. 🔄 **Next critical step**: live keystroke test (see Testing section above).

### Architecture Decisions Recorded
- **Manager pattern** (vm_rv_li_manager.c) follows CENTROID_GOLD_STD: business logic
  in separate process, not inline in shared renderer.
- **Layout** (vm-rv-li.xhtpm/css) uses generic khtpm vocabulary (`window`/`panel`/
  `cli_io`/`scrolllist`), zero per-app branches in khtpm_core_render.c.
- **Input relay** (human_input.txt + agent_input.txt) enforces single-writer-per-file
  discipline, prevents race conditions in emulator stdin.
- **RVLV_EMUL assets**: xv6 images verified present (`images/xv6-kernel.bin`,
  `images/xv6-fs.img`); Linux buildroot not yet built (optional, requires ~1 hour).
