# tearit-hq — data, tracking & legal implementation notes

> **Status: EXPLORATION / DECISION-NEEDED, 2026-09-02.**
> This is a working design doc, **not legal advice**. Before *any*
> personal data leaves a user's machine — even an email address, even a
> signup ping to a private channel — this needs review by a real lawyer
> in the jurisdiction(s) we ship to. The goal here is to lay out the
> real choices and a safe-by-default path so that review is short and
> cheap, not open-ended.
>
> Companion to `BOOK/07-install-and-ship/SECURITY.md` §5 (accounts /
> signup / login for strangers).

---

## 0. What the installer collects **today**

Nothing. Stating this plainly because it's the safe baseline and every
option below is measured against it:

- `install.sh` / `bootstrap.sh` make **no network calls except** the
  payload download from `github.com` (GitHub's own logs apply; we
  receive nothing).
- Signup (`userpal_create_account`) writes `users/<id>/profile.txt`
  **on the user's own disk**. It is never transmitted anywhere.
- No analytics, no telemetry, no crash reporting, no unique install
  ID, no phone-home. There is no server.

So right now we have **zero legal obligations around user data** beyond
GitHub's. The moment we collect or transmit anything, that changes —
this doc is about doing that deliberately.

---

## 1. The questions to answer (owner)

1. **Do we want to know who our users are at all yet?**
   - (a) No — keep it zero-collection through the friend phase.
   - (b) Anonymous count only — "N installs, M signups", no identities.
   - (c) Identified — name/email/handle tied to a signup.
2. **At signup, do we send anything off-machine?** To where? A private
   Discord webhook? A private GitHub repo (issues/commits)? A real
   endpoint? What exactly is in the payload?
3. **Email: collect it? validate it?** Before install, at signup, or
   never (yet)? Double-opt-in, or just store what they type?
4. **Who are we shipping to when this goes live?** Owner and a friend
   only → almost nothing is required. Public strangers, especially any
   in the EU/UK/California → a real privacy policy, lawful basis,
   deletion mechanism, and age handling all become mandatory *before*
   the first stranger signs up.

---

## 2. What is lawful to collect — and what attaches when you do

Rough map (confirm with counsel; this is orientation only):

| Data | Lawful to collect? | Obligations it triggers |
|---|---|---|
| Anonymous install/run count (no IP stored, no ID) | Generally yes, low risk | Minimal; still disclose it |
| Chosen username / display name | Yes, with notice | Privacy notice; deletion on request |
| Email address | Yes, with consent + purpose stated | Privacy notice; lawful basis (consent or contract); deletion/access rights; breach-notification duty; keep it secured |
| IP address | Is personal data under GDPR | Same as email; needs a stated purpose (e.g. abuse prevention) and retention limit |
| Anything about a **minor** | Special category — often needs verifiable parental consent (COPPA <13 US; GDPR 13–16 depending on country) | Age gate at signup; do not collect if under-age; documented |
| Passwords | You're not "collecting" them to keep — you store a **hash** | Never store plaintext; use a real KDF (argon2id/bcrypt/scrypt); this is a security duty independent of privacy law. **Check today's `userpal_create_account.c` — SECURITY.md §5 flags this as unverified.** |

**Principle to design to: data minimization.** Every field we collect
is a field we must secure, disclose, justify, retain-limit, and delete
on request. Collect the fewest fields that make the product work at the
current phase.

---

## 3. The "send signup to a private Discord / GitHub" idea — analysis

The instinct (get a ping when someone signs up) is reasonable. The
mechanics matter:

- **A Discord webhook URL is a secret.** If it ships inside `install.sh`
  or any payload file, it is public the moment the repo is public —
  anyone can spam it or scrape every signup that flows through it. So a
  webhook baked into the client is **not acceptable** for real user
  data.
- **A private GitHub repo as a sink** (client opens an issue via the
  API) needs a token with write scope on the client — same exposure
  problem, worse (a leaked token can do more than a webhook).
- **The clean version keeps secrets off the client.** Two ways: (a) a
  small endpoint we run that holds the credentials, or (b) a
  Backend-as-a-Service whose client key is *designed* to be public and
  whose server-side rules constrain it (Supabase anon key + row-level
  security). Option (b) is much less infrastructure — see §3.5. Either
  way it still needs TLS, a retention policy, and a privacy policy
  naming where data lands.
- **Interim, no-infrastructure option**: don't transmit at all. Signup
  stays local. If the owner wants visibility, add an explicit,
  opt-in `hq export-signup` command the *user* runs that prints a
  blob they can voluntarily paste to us. Consent is unambiguous
  because they physically did it.

Recommendation: **no automatic phone-home until there's a server-side
endpoint and a published privacy policy.** Until then, either (b)
anonymous count via GitHub's own release-download stats (already
available, we collect nothing ourselves) or the opt-in manual export.

### 3.5 So — Supabase or something, not GitHub? Yes. Concrete options.

GitHub is a code host, not a data backend — there is no way to make a
public client write to it without shipping a credential that leaks. A
**Backend-as-a-Service (BaaS)** solves the exact problem: the client
holds only a *public* key, and server-side rules decide what that key
can do. All of the following have a real free tier; all are
open-source or self-hostable so we are not locked in.

| Service | Free tier (verify current limits) | What it gives us | Cost/effort | Notes |
|---|---|---|---|---|
| **Supabase** (recommended) | ~2 projects, 500 MB Postgres, 50k monthly active auth users, social + email auth, edge functions; projects pause after ~1 wk idle on free | Postgres + auto REST/realtime + **Row Level Security** + **Auth with email confirmation & magic links** + storage | Low — mostly config | The client ships the **anon key, which is designed to be public**; an *insert-only* RLS policy on a `signups` table lets anyone submit but nobody read. Also covers the *account system itself* (§ SECURITY.md §5), not just the ping. Open-source, self-hostable later. US/EU region choice. Needs a DPA + privacy-policy mention. |
| **PocketBase** | Free software; you run it (a $4–6/mo VPS or a free Fly.io/Render instance) | Single Go binary: SQLite + REST + auth + admin UI + file storage | Low-medium — you own a small box | Cheapest long-term, full data ownership, trivial backups (one file). No vendor at all. You handle uptime + TLS (Caddy/Fly does TLS for you). |
| **Firebase / Firestore** (Google) | Generous (Spark plan): 1 GiB store, 50k reads/20k writes per day, email/OAuth auth | Similar to Supabase | Low | Google lock-in, harder to self-host, data in US by default. Same public-config-key + security-rules model. |
| **Cloudflare** (Workers + D1 + Turnstile) | 100k requests/day, D1 SQLite 5 GB, free CAPTCHA (Turnstile) | You write a ~30-line Worker as the intake endpoint; D1 stores rows | Medium — more assembly | Best if we want a *minimal* endpoint and strong built-in bot protection, and don't need a full auth system yet. |
| **Airtable / Google Forms** | Yes | A table / form that collects a beta **waitlist** | Trivial | Fine for "email us to get early access" only. **Not** an auth backend, and the data still sits in a US SaaS you must disclose. |

**What this does *not* remove:** wherever the data lands, if it
includes an email or IP it's still personal data — we still need the
privacy policy, a lawful basis, a data-processing agreement with the
provider, a stated retention period, and a deletion path. BaaS makes
the *plumbing* safe; it doesn't make the *obligations* go away.

**Concrete recommendation:**
- **Phase B/C backend: Supabase.** One service covers the account
  system, email verification, and the signup record, with a
  public-safe client key. Start with an `signups` table + insert-only
  RLS + Turnstile/hCaptcha in front to stop mass fake inserts.
- Keep **PocketBase** as the fallback/exit plan if we ever want to
  pull everything onto our own box — the data model ports cleanly.
- **Do not** use a Discord webhook or GitHub token in the client, ever.
  A Supabase edge function or database webhook can post to Discord
  *server-side* if the owner still wants the ping.

### 3.6 Is Discord itself a viable data store? (short: no — it's a notifier)

People do abuse Discord as free storage (bots that stash blobs in
channels/attachments). For *our* case — signups, emails, account data —
it is the wrong tool:

- **No query / no structured deletion.** "Delete every record for this
  email" (a legal right) means scrolling a channel deleting messages by
  hand. Not a system of record.
- **Not a data processor you can contract with.** No DPA, no defined
  storage region, no security attestation for *your users'* PII.
  Storing third-party personal data in a Discord channel likely breaks
  Discord's own ToS and gives you no compliance story.
- **Access control is coarse.** Anyone in the channel — a future mod, a
  leaked invite — sees every record.
- **Rate-limited, no durability guarantee.** Fine to drop a ping, not
  fine to lose a signup.

**Where Discord *is* good:** a human-facing **ops alert** — "🎉 new
signup (#128)", "build failed", "store submission pending review" —
posted **server-side**, carrying a count or an opaque id, **never** the
person's email/IP. Treat it like a Slack/email notification, not a
database. The real record lives in Supabase/PocketBase; Discord just
tells you something happened.

---

## 4. Email validation — options, if/when we collect it

Ordered by cost:

1. **Syntax + MX check only** (offline-ish): reject obviously-bad
   strings, do a DNS MX lookup on the domain. Cheap, catches typos, does
   **not** prove the address is real or owned by the signer.
2. **Double opt-in**: send a confirmation link with a token; the
   account is "unconfirmed" until they click. This is the real bar for
   "we have a valid, consenting email" and is what most jurisdictions
   effectively expect before you email someone. Needs the server-side
   endpoint + an email-sending path (SES/Postmark/etc.).
3. **Before install vs at signup**: collecting email *before* download
   (a landing-page form) is a marketing-funnel choice, not a technical
   one — same legal obligations, and it gates trying the product on
   giving us data, which raises the consent bar. Prefer **at signup,
   optional**, with a clear "why" ("account recovery only; we won't
   email you otherwise unless you opt in").

**Do we do it yet?** No — nothing validates or transmits email today,
and we shouldn't add it until §3's endpoint exists. When we do: start
at level 1, move to level 2 before any stranger-facing launch.

---

## 5. Proposed phased plan (matches owner's test-user order)

### Phase A — owner + friend (now)
- **Collect nothing off-machine.** Keep the zero-obligation baseline.
- Ship this doc + a plain `PRIVACY.md` stub in the install repo saying
  exactly that ("this build sends us nothing except the GitHub
  download").
- **Do** fix the local-security item now regardless of phase: verify
  password hashing in `userpal_create_account.c`; if it's plaintext,
  that's a real bug to fix before *anyone* else runs it (SECURITY.md
  §5).
- Add an age-affirmation checkbox to signup UI copy now ("I am 16 or
  older") even while local-only — cheap, and it's the hook we need
  later.

### Phase B — small controlled group (friends)
- Optional, opt-in `export-signup` manual blob (no auto-transmit).
- Anonymous aggregate only: read GitHub's release/download counts.

### Phase C — public strangers (gated on all of the below existing)
1. A real, lawyer-reviewed **Privacy Policy** and **Terms of Service**,
   linked from the installer and the signup screen.
2. A **lawful basis** chosen per field (consent for email/marketing;
   legitimate interest or contract for the account itself).
3. A **backend** we control — Supabase (recommended, §3.5) or a small
   self-run endpoint — with TLS, access logs, credentials never in any
   client, and bot protection (Turnstile/hCaptcha) on the signup path.
4. A **deletion + access path** (`hq account delete` that reaches the
   endpoint; a documented manual process as backup) — GDPR/CCPA give
   users this right and we must be able to honor it.
5. **Retention limits** written down (e.g. "unconfirmed emails purged
   after 30 days; account data kept until deletion requested").
6. **Age gate** enforced, not just affirmed; no data collection from
   self-declared minors.
7. **Breach process**: who is notified, how fast (GDPR: 72h to the
   supervisory authority).

---

## 6. Files this implies (create when the phase needs them)

- `PRIVACY.md` — in `tearit-install`. Phase A version is 5 lines: "sends
  nothing". Grows with each phase.
- `TERMS.md` — Phase C.
- `DATA-MAP.md` — the authoritative per-field table: what, why, lawful
  basis, where stored, retention, who can access. Start it at Phase B.
- signup UI copy — the consent/age/why-we-ask text, versioned so we can
  prove what a user agreed to and when.

---

## 7. Immediate, phase-independent actions

1. **Verify password storage** in `0.user-pal👤️/00.login-signup/ops/
   userpal_create_account.c`. Plaintext or weak hash = fix now.
2. Add `PRIVACY.md` (Phase A "we send nothing" version) to
   `tearit-install` and link it from `README.md` + the FAQ.
3. Add the age-affirmation line to signup copy.
4. **Decide §1.1** (do we want identities yet) — everything else
   branches off that answer.

## 8. Cross-references
- `BOOK/07-install-and-ship/SECURITY.md` §5 — account/auth gaps.
- `BOOK/07-install-and-ship/PHONDO_INSTALL_IDEAS.md` §4 — "accounts
  across machines / real backend" named as undesigned.
- `BOOK/07-install-and-ship/USER-JOURNEY-COMPLETION-GRAPH.md` step 3 —
  the cursword-driven signup flow this consent copy would live in.
