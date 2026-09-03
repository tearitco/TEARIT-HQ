# 🔒 Long-term security — shipping to strangers

**Status: EXPLORATION, 2026-09-02.** Companion to `PHONDO_INSTALL_
IDEAS.md`. That doc covers install/store shape; this one covers the
real, concrete security concerns that only start to matter once "a
user" means a stranger off GitHub, not the owner or a friend. Nothing
here is fixed yet — this is the list of real questions to settle
before shipping past friends-and-family, written down now while the
concerns are fresh (the network-browser review that surfaced several
of these).

## 1. Why this matters now, not later

Today, every real "user" of this house is someone the owner trusts
completely, running code the owner (or an agent working for them)
wrote or reviewed. The moment install/store ships to strangers, three
trust boundaries that don't exist today become real:

1. **The install itself** — a stranger runs a script that compiles and
   runs C binaries with their own real user permissions.
2. **Store content** — a stranger's install pulls "toys/pals" (real,
   compiled C ops) from a catalog the owner may not have personally
   reviewed line-by-line.
3. **Fetched network content** — apps like the network browser fetch
   and render arbitrary third-party content (web pages, and soon,
   images/video/JS) by design. That content is adversarial by default
   the moment a stranger can type any URL into it.

None of these are hypothetical — the network-browser review just
found the second phase of #3 already in progress (a Duktape JS eval
op, `nb_js_eval.c`, meant to run arbitrary fetched page JavaScript).

## 2. Concern: the install script itself

- The install is, mechanically, "download a script, run it, it
  compiles and runs C code as you." This is normal for a dev tool but
  a real trust decision for a stranger. At minimum: the install script
  itself should be small, readable, and auditable in one sitting — not
  something that shells out to fetch and immediately `eval`/`exec`
  more code from elsewhere without the user seeing what.
- **Compile step = real code execution.** Every op binary in this
  house is real C compiled by the installer with the user's own
  toolchain. A malicious or buggy op is not sandboxed by anything
  today — it runs with full access to whatever the user's shell can
  do. This is fine for a house the owner fully controls; it is a real,
  open question for a stranger's install.
- **No signing/verification story yet.** Nothing today checks that the
  code being installed matches what was reviewed/approved — an install
  script pulling from a URL has no way to detect a tampered payload in
  transit or a compromised source.

## 3. Concern: store content (third-party toys/pals)

Directly from the owner's own stated plan: toys/pals may be individual
GitHub repos, or entries in a central "approved" catalog, reviewed
manually for now, automated (harness-based) later.

- **A store item is compiled C, not a sandboxed script.** Unlike a
  browser extension or a mobile app, there is no runtime sandbox
  around what an installed op can do — it can read/write any file the
  installing user can, make network calls, spawn processes. "Manual
  review" today means literally reading the C source before approving
  it — that's a real, continuing cost, not a one-time gate, and it
  does not scale past a small number of trusted contributors without
  either (a) real automated static analysis / capability restriction,
  or (b) accepting that "approved" only ever means "a human read this
  specific version," with no guarantee about future updates to the
  same repo.
- **Repo-per-toy vs. central catalog changes the attack surface.**
  Individual repos mean a compromised or malicious maintainer can push
  a bad update to their own repo at any time, silently, to anyone who
  already installed it, unless the install pins a specific commit/tag
  rather than tracking a branch. A central catalog repo (the owner's
  stated default direction) gives a single choke point to review
  updates before they reach users — a real, concrete argument for the
  central-catalog-as-default decision already made in `PHONDO_
  INSTALL_IDEAS.md`, worth stating explicitly here as a security
  reason, not just a simplicity one.
- **Namespace/identity risk**: nothing yet stops a store item from
  claiming a trusted-sounding name. A "verified publisher" concept
  (even a simple one — a list of GitHub usernames/orgs the owner has
  personally vetted) is worth having before the catalog is genuinely
  open to outside submissions.

## 4. Concern: network-fetched content (the network browser, and anything like it)

The network browser fetches arbitrary URLs a user types in. Once
strangers use this, "arbitrary" means "adversarial." Real, concrete
sub-concerns found during the friend's own real work on this app:

- **SSRF / local-network fetch**: `curl`-based fetching with no URL
  restriction can be pointed at `localhost`, `169.254.169.254`-style
  metadata endpoints (if ever run in a cloud VM), or other machines on
  a user's own LAN (their router admin page, other devices). A real,
  cheap mitigation: block fetches to private/loopback/link-local IP
  ranges by default, with an explicit opt-in for anyone who genuinely
  wants a LAN-browsing feature.
- **No response-size or time limit found yet** on the curl fetch — a
  malicious server could serve an effectively infinite response
  (a "decompression bomb"-style resource exhaustion) or hang the
  connection open. Real fix: a hard byte cap and a real timeout on
  every fetch, not just a happy-path assumption the response is small.
- **Media parsing is a real, historical source of native-code CVEs.**
  The planned `nb_media_to_sprite.c` (JPEG/PNG/GIF/WEBP via `stb_
  image`) inherits whatever bugs exist in `stb_image` itself, run
  against attacker-controlled bytes, in a C process with no sandboxing
  layer around it. `stb_image` is a real, widely-used, reasonably
  audited library, but "reasonably audited" is not "immune" — a
  malformed image is a classic attack vector against exactly
  this kind of code path. At minimum: keep `stb_image` genuinely
  up to date, and consider running media decode in a separate,
  low-privilege child process (already a step this house's process
  model makes natural — the op-per-command pattern already isolates
  by binary) so a decoder crash/exploit doesn't take down the whole
  browser or reach the rest of the user's session.
- **Arbitrary JavaScript execution is the single biggest new risk
  surface being proposed.** `nb_js_eval.c`'s own notes describe "an
  isolated Duktape eval op, 3s timeout, window/self/globalThis" — a
  reasonable *starting* shape, but "isolated" needs a precise,
  verified definition before this ships to a stranger, not an assumed
  one. Real questions to answer before this is safe to expose to
  untrusted page content:
  1. **File system access**: does the Duktape context have any bound
     function that can read/write/exec anything on disk? If yes, any
     page's JS can act with the user's own file permissions. The op
     should expose zero such bindings by default.
  2. **Network access from within JS**: can page JS itself issue new
     fetches (its own XHR/fetch-equivalent), separate from the
     browser's own address-bar-driven fetch? If so, that's a second,
     JS-driven SSRF surface on top of §4's curl concern, with less
     visibility to the user.
  3. **Resource limits beyond the 3s timeout**: memory caps, recursion/
     stack limits — Duktape supports both; confirm they're actually
     configured, not just the timeout.
  4. **What can JS actually observe/change in the rendered page or
     the browser's own state?** — an `alert()`-equivalent or a DOM-
     like mutation API is exactly where a real XSS-style attack against
     the BROWSER ITSELF (not just the page content) would live if the
     JS bridge exposes anything beyond the current page's own data.
  5. This is precisely the class of decision `CENTROID_GOLD_STD.md`'s
     source material (`sep-1-events-SOS.md`'s Tier 4 list) already
     flagged for events: **"Script / Plugin Command (arbitrary code
     execution) — this house already has a real op-binary-per-command
     pattern specifically so events never need to eval arbitrary text;
     adding a raw 'run this code' command would be a deliberate
     architecture change... check in before building this one
     specifically."** The network browser's JS eval op is the exact
     same category of decision, just for page content instead of
     event scripting — it deserves the same explicit sign-off, not a
     default yes because it arrived as a working demo.

## 5. Concern: accounts / signup / login for real strangers

- Today's login/signup (`0.user-pal👤️/00.login-signup/`) is a real,
  working, single-machine account system — it has never been reviewed
  for what a stranger-facing product needs: password storage (is
  anything hashed today, or stored plain in a flat file, matching the
  house's own "everything is a real file" convention taken literally?
  — **this needs a direct check before shipping,** the file-based-
  state philosophy is right for app state, wrong for secrets if
  applied naively), rate-limiting signup/login attempts, and basic
  abuse resistance (mass fake account creation against a store/catalog
  that lets users publish content).
- No password-reset, no email verification, no session-expiry story
  exists yet, as far as this review found — all real gaps for a
  stranger-facing product, not necessarily all needed on day one, but
  worth naming now rather than discovering the gap live.

## 6. What this doc is NOT saying

- Not saying any of this blocks the CLI-bootstrap/local-install work —
  that's still squarely "you, then a friend" territory per the owner's
  own stated test-user order, and none of these concerns are urgent
  for that phase.
- Not proposing specific fixes yet for most of these — this is a
  concern inventory to work from, not a design doc. Several items
  (URL/IP filtering, fetch size caps, JS sandbox verification) are
  small, concrete, and could be scoped into real tasks whenever this
  becomes the active work; others (store review model, account
  security) need a real decision conversation first, same as install/
  store did.

## 7. Cross-references

- `PHONDO_INSTALL_IDEAS.md` — the install/store shape decisions this
  doc's §3 security reasoning builds on.
- `08-roadmap/design-docs/sep-1-events-SOS.md` — the existing house
  precedent (§4.5 above) for treating "arbitrary code execution" as a
  deliberate, check-in-first architecture decision, not a default.
- `02-architecture/HTML-MEDIA-AND-SCRIPTING.md` — the rendering-side
  design this security doc's §4 concerns apply to directly (image/
  video handling, the JS eval op's actual integration shape).
