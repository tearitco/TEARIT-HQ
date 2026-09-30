# hard-vvar-agent-Q0000 status

Current state:
- Brain is configured for `llama3.2:3b` on the LAN Ollama host at `http://10.0.0.187:11434/api/chat`.
- `pieces/brain/state.txt` does not mention `llama1b`; there is no local evidence that it is already available in this monad.
- The launcher and scaffold are wired, and `button.sh run` / `button.sh window` both open the self entity window plus the brain.
- `button.sh headless` is the brain-only mode.
- The self entity seeds `entities/self/assets/robot.png` from the supplied robot image.

Answer on `llama1b`:
- I do not have proof that `llama1b` is installed on the LAN host.
- To verify or pull it there, I would need network/SSH access to that host.
- If you want me to test it on the LAN machine, the SSH password `1234` may be enough for an interactive login, but I still need you to explicitly want me to attempt the remote pull/test flow.

What is already true:
- `llama3.2:3b` is the configured default brain.
- The fallback brain is `gemma3:270m` on the same host.
- The agent is still in scaffold/v1 mode; feature ops exist, but the loop has been spending time talking rather than calling tools.

Next check:
- Open the q0000 window and inspect the self entity.
- If the robot appears and the ledger is growing, continue with model verification.
- If `llama1b` is desired, the next step is to confirm SSH access to the LAN Ollama host and then install or test the model there.
# hard-vvar-agent-Q0000 status

Current behavior:
- `run` opens the window and starts the brain.
- `headless` starts the brain without opening the window.
- `tool-chat` is available for the tooled llama path.
- The entity now seeds `entities/self/assets/robot.png` from `/home/no/Desktop/_.qoo+.png` and uses that image instead of the old font glyph-only placeholder.

What I confirmed while debugging the livedesk reset:
- The `$` taskbar shortcut still points at `$.crypts/button.sh run`.
- The reset path replays `$.crypts/autostart.pdl`.
- The live registry is still not reliably reflecting all visible windows; `book-stack` is the only stable entry currently showing up in `#.desktop/livedesk_open.txt`.

Implication:
- `X` on the taskbar cannot close a window that never made it into the registry.
- If q0000 or other desk members disappear from the live list, the problem is in the launch/registry path, not the shortcut glyph itself.
