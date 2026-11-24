# tearit web chain (experimental, not shipped)
A static page (`docs/`, GitHub Pages) that keeps a copy of the tearit web chain in your browser: verify every block, mine, faucet, send, post messages, export and import your copy, merge by the fork rule (most work wins).
- `docs/chain.js`: one pure ES5 file, the same bytes run in the page and in the house harness (case `web_chain`, graded against the real C chain ops).
- `docs/commands.json`: data-only command list for command lines and agents.
- Submit without a server: open an issue labelled `tx` with rows (`TX|from|to|amount|ts|id`, `MSG|from|channel|text|ts`, `FAUCET|wallet|amount|ts`), one per line; the steward workflow verifies, mines one block into `docs/chain/latest.txt` and closes the issue.
- Install the house: `curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- tearit-hq`
