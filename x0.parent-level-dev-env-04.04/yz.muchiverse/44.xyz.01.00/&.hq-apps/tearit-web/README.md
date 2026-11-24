**[Play it in your browser: https://tearitco.github.io/tearit-web/](https://tearitco.github.io/tearit-web/)**  ([the ranch](https://tearitco.github.io/tearit-web/ranch.html) · [the chain explorer](https://tearitco.github.io/tearit-web/chain.html))

# TEAR-IT Ranch (tearit web chain, experimental, not shipped)
A Monster-Rancher-style pet game that runs from static files (GitHub Pages) on a blockchain you keep in your browser.
- **Hatch** a pet from your own words (the same words always hatch the same pet, one of ten elements), or **upload** a save/world, or fetch the published one.
- The **pet is the wallet**: faucet and mining pay it, food, drills, upgrades and cup entries are paid from it. Feed, train, rest, fight the ranked AI ladder (E to S, beat the rank below to open the next) and other pets, buy and use items, upgrade the gym and barn, walk it around the 12x8 ranch grid (2D or 3D view, up to six pets out at once), freeze it, list it on the market, trade it. Every action is a row; the page re-checks every block with the same rules the C tools use.
- Pages: `docs/index.html` (landing), `docs/ranch.html` (the game), `docs/chain.html` (explorer, coins, messages, install). Rules: `docs/chain.js` (ONE pure ES5 file, graded by the house harness case `web_chain`). Art: placeholder (our own art comes later).
- Submit without a server: an issue labelled `tx` with rows, one per line (`TX|`, `MSG|`, `FAUCET|`, `PET|`, `FEED|`, `TRAIN|`, `REST|`, `COMPETE|`, `UPGRADE|`, `FREEZE|`, `THAW|`, `TRANSFER|`); the steward workflow verifies each and mines one block into `docs/chain/latest.txt`.
- Install the house: `curl -fsSL https://raw.githubusercontent.com/tearitco/tearit-install/main/install.sh | sh -s -- tearit-hq`
