# HARNECIENT: Complete User Guide

## Part 1: What You've Just Learned (Training Summary)

Welcome. Over the past 20 days, you've been learning the HARNECIENT way — a new approach to building systems where small language models become powerful tools through intelligent harnesses, not by pretending to be something they're not.

### The Core Philosophy

Everything you need to know starts with this truth: **If it's not in a file, it's a lie.** We don't trust memory. We don't trust vibes. We write reality down, then prove it.

HARNECIENT is the practice of treating small models as text engines, not tool routers. A 270-million-parameter model from Ollama can't magically understand how to call your tools. But a harness around that text output can. We extract the patterns the model produces, we interpret them reliably, and we route them to the right action. The model provides the spark. We provide the work.

### The 20 Days: A Quick Recap

**Days 1-3: Foundations**
- Day 1: You learned what HARNECIENT means and why it matters
- Day 2: You met the House itself — the livedesk taskbar, the 15-cell interface that runs everything
- Day 3: You learned about relays — plain text files that hold key presses and drive the entire system

**Days 4-5: The Hack**
- Day 4: The problem — small models hallucinate tool formats. They don't understand tools
- Day 5: The solution — the HARNECIENT hack. Five components, each one load-bearing

**Days 6-7: Proof**
- Day 6: Never classify. Always describe
- Day 7: We proved it works — even the tiny 270m model can edit real code 9 for 9

**Days 8-10: The Storefront**
- Day 8: H.AI — a single C file that renders a chat interface and sends text to Ollama
- Day 9: A four-phase plan to wire harnesses into the live model
- Day 10: The taskbar is your interface to power

**Days 11-13: The Harnesses**
- Day 11: The four laws of a living harness
- Day 12: The code-tools harness — how we extract tool calls and run real commands
- Day 13: Relay harnesses — the skeleton used everywhere

**Days 14-15: The Masterclass**
- Day 14: How to prompt agents so they don't burn your tokens
- Day 15: Every rule came from watching real sessions burn money

**Days 16-20: The Product**
- Day 16: Events — games are, at their core, events
- Day 17: The database is not a black box — it's a file cabinet
- Day 18: Everything so far exists for the games
- Day 19: A window into the near future
- Day 20: The vision — a factory agents and humans can both operate

### The Nautilus Spiral

The same logic applies at every scale. A galaxy contains solar systems. A chest contains gold. The rules are identical. That's the Harnecient pattern: φ = 1.618. The spiral continues.

---

## Part 2: Installing HARNECIENT (Your System)

### What You're Installing

HARNECIENT is not software you download. It's a practice built on:
1. **Livedesk** — the 15-cell interface where everything runs
2. **TPMOS** — the Piece Method Operating System, where files are reality
3. **Relay harnesses** — text-based scripting that drives behavior
4. **Small models** — Ollama-compatible models (Gemma, Stable Code, Mistral)
5. **Your games** — structured event files that run on this foundation

When you install HARNECIENT, you're creating a fresh user home where you can:
- Create an account and sign up
- Create games using event files
- Play games built by you or others
- Invite others to play

### Installation: Step-by-Step

#### Step 1: Prepare Your System

You need:
- A Linux machine (Windows support coming soon)
- Bash shell
- About 2GB of free disk space
- An Ollama instance running (or we can install it)

Check if you have the installer:

```bash
ls -la ~/xyz-installer-dev/xyzos-starter-install.sh
```

If you see that file, you're ready. If not, this installer may need to be set up by your admin.

#### Step 2: Run the Installer

The installer is non-destructive. It will refuse to run if you already have a `~/xyzos` directory. That keeps you safe.

```bash
bash ~/xyz-installer-dev/xyzos-starter-install.sh
```

The installer will:
- Read from the dev tree (never writes to it)
- Create a fresh `~/xyzos/` directory
- Copy the login and avatar apps
- Compile them
- Set up paths
- Create a launcher script

You should see output like:
```
=== xyzos install v1 complete ===
    root:   ~/xyzos
    boot:   ~/xyzos/button.sh
    login:  ~/xyzos/apps/00.login-signup
    avatar: ~/xyzos/apps/01.avatar-creation👤️
```

#### Step 3: Verify the Installation

Check what was created:

```bash
ls ~/xyzos/
```

You should see:
- `apps/` — login and avatar apps
- `app-store/` — ledger of installed apps
- `xyzfs/` — user file system
- `button.sh` — launcher
- `paths.pdl` — the pointer file (logical name → real path)

#### Step 4: Boot for the First Time

```bash
~/xyzos/button.sh
```

You'll see:
```
Not logged in.
```

This is correct. The system is waiting for you to create an account.

---

## Part 3: Using HARNECIENT

### Creating Your Account

When you boot the system, you're at the login screen. Here's how to create an account:

1. **You'll see:** A prompt asking for "User ID"
2. **Type:** A username you want (e.g., `alice` or `alex-dev`)
3. **Press:** Enter
4. **You'll see:** A prompt for "Display Name"
5. **Type:** How you want to appear to others (e.g., `Alice` or `Alex Dev`)
6. **Press:** Enter
7. **Button:** Select "Create Account"

The system will:
- Generate a unique UUID for you (stored, but you don't manage it)
- Create a file with your profile
- Create your private xyzfs home
- Log you in

You should see:
```
Logged in as: alice
```

Congratulations. You have an account.

### Understanding Your File System (xyzfs)

Everything you own lives in one place:

```
~/xyzos/xyzfs/users/<your-uuid>/
```

Inside that directory:
- `meta.txt` — metadata about you
- `home/` — your home directory
- `projects/` — where you build games
- `saves/` — where you save game progress

Your files are plain text. You can edit them, back them up, move them. They never leave this directory. Everything is local.

### Playing a Game

Games are pre-built event files that someone (you or someone else) created. To play a game:

1. **Boot the system:** `~/xyzos/button.sh`
2. **Log in** with your account
3. **Look for the game in your home**
4. **Select "Play"**
5. **Follow the prompts** the game designer created

The game runs through a series of events: descriptions, choices, outcomes. Each step reads from a file. If something goes wrong, the file is still there for you to inspect.

### Creating Your First Game

Games are event files. An event file is a plain-text `.pdl` file that describes what happens when the player interacts with the game.

Basic game structure:

```
SECTION      | KEY              | VALUE
----------------------------------------
META         | piece_id         | my_first_game
META         | version          | 1.0

EVENTS       | start            | welcome_message
EVENTS       | choice_a         | go_north
EVENTS       | choice_b         | go_south

WELCOME      | text             | Welcome to my game. Go north or south?
WELCOME      | choices          | North|South

NORTH        | text             | You found treasure!
NORTH        | end              | true

SOUTH        | text             | You found danger!
SOUTH        | end              | true
```

To create your first game:

1. **Create a new file:** `~/xyzos/xyzfs/users/<uuid>/projects/my_game.pdl`
2. **Write your event structure** (see the template above)
3. **Save the file**
4. **Boot the system**
5. **Look in your projects directory**
6. **Select "Play"**
7. **See your game come to life**

The system parses the `.pdl` file and renders each event as the player moves through your game.

### Inviting Others to Play

Your games are files. To share a game:

1. **Copy your game file** (e.g., `my_game.pdl`)
2. **Send it to someone else**
3. **They place it in their projects directory**
4. **They boot the system and play**

No server. No cloud sync. No accounts to manage on their end. Just files.

### Logging Out and Back In

When you log out:

```
Press Ctrl-C (or select "Logout")
```

The system writes your state to a file. Your UUID is stored. Your home is still there.

When you log back in:
1. Boot the system: `~/xyzos/button.sh`
2. Enter your User ID (the same one you used before)
3. The system finds your UUID
4. Your home is restored
5. Your games are waiting

Logging in is stateless. It's just a file lookup.

### Backing Up Your Games

Because everything is files, backing up is simple:

```bash
cp -r ~/xyzos/xyzfs/users/<your-uuid>/ ~/backups/my-xyzos-backup-$(date +%F)
```

You now have a complete backup of your account, your home, and all your games.

### Troubleshooting

**Q: I see an error about missing paths.pdl**
A: Your install may be incomplete. Re-run the installer into a fresh `~/.xyzos-fresh` directory and compare the directory structures.

**Q: My game file won't load**
A: Check the `.pdl` file format. It's very strict about the pipe-delimited format. Use `cat` to inspect it:
```bash
cat ~/xyzos/xyzfs/users/<uuid>/projects/my_game.pdl
```

**Q: How do I reset my account?**
A: Delete your user directory and log in again as a new account:
```bash
rm -rf ~/xyzos/xyzfs/users/<your-uuid>/
```

---

## Part 4: Philosophy for Game Makers

When you build a game in HARNECIENT, remember:

1. **State is a file.** If it's not written to disk, it doesn't exist. Plan around that.
2. **Events are atomic.** One event = one choice point or outcome. Keep them small.
3. **Players are explorers.** Design so they can inspect `.pdl` files and understand the game's logic.
4. **Simplicity wins.** The more text your game is, the slower it feels. Make events punchy.
5. **The Nautilus Spiral applies.** A small game can be nested inside a large one. A big world is just many small pieces.

---

## Part 5: Next Steps

You now understand:
- How HARNECIENT works (20 days of training)
- How to install it (5 minutes)
- How to use it (create accounts, play games, make games)
- How to share it (copy files)

What to do next:

1. **Install the system** on your machine
2. **Create your first account**
3. **Create a small test game** (3-5 events)
4. **Play it yourself**
5. **Invite someone to play**
6. **Build bigger**

The tools are ready. The harness is proven. The rest is up to you.

Remember: **If it's not in a file, it's a lie.** Write it down. Then prove it works.

---

## Appendix: File Locations Reference

For quick lookup:

| What | Where |
|------|-------|
| Install root | `~/xyzos/` |
| Your login app | `~/xyzos/apps/00.login-signup/` |
| Avatar app | `~/xyzos/apps/01.avatar-creation👤️/` |
| Your home | `~/xyzos/xyzfs/users/<uuid>/home/` |
| Your projects | `~/xyzos/xyzfs/users/<uuid>/projects/` |
| Your saves | `~/xyzos/xyzfs/users/<uuid>/saves/` |
| Path map | `~/xyzos/paths.pdl` |
| Installed apps | `~/xyzos/app-store/installed_apps.pdl` |
| Current login | `~/xyzos/apps/00.login-signup/current_login.txt` |
| Account records | `~/xyzos/apps/00.login-signup/users/<username>/profile.txt` |

---

**End of Guide**

Remember: The house rule that keeps this whole world honest is simple. **If it's not in a file, it's a lie.** You've learned how to build in this world. Now go build something.
