# wsr-pal ("Wall $treet Race") — overview + editor-ops rewrite scope

Localized doc for this project specifically. Cross-reference:
`2.muchi-verse/GRAND-ARCHITECTURE.md` (the family-wide plan this project
now has a note in). Written after a breadth-first survey of the
existing ~306-file codebase - not everything was read (see "What
wasn't checked" at the bottom); treat this as oriented-but-incomplete,
not exhaustive.

**Where the real content lives**: the actual project is NOT at this
directory's top level - it's one level down, in a directory with a
decorated/garbled name:
`wsr-pal/Mar$.$treetRace.wsr]Q]k32/`. That's almost certainly an
artifact of how it got exported/zipped at some point, not an
intentional name. Consider renaming it to something plain
(`wsr-pal/game/` or similar) during the eventual rewrite - not done
here, this doc just documents where things are today.


## 1. What this actually is, architecturally

**Not built on pal/prisc+x, despite looking related at a glance.** No
`.pal` script file exists anywhere in the tree, no bytecode VM, no
opcode interpreter. What it DOES share with the pal family is cosmetic:
one C source per verb, compiled to a same-named binary in a `+x/`
folder (confirmed 1:1 for every root `.c` file). The actual execution
model is a single long-running `game.c` binary (lines 311-485) that
polls the keyboard directly (`select()`/`kbhit()`, lines 93-98) and
dispatches via a big menu-number `switch` that literally shells out -
`system("./+x/foo.+x")` - to the other binaries (16 of ~35 root `.c`
files contain `system()` calls). This is a conventional "main loop +
subprocess menu" CLI game, not a VM running scripts. The one
orchestrator-shaped file
(`0.platform.orb.args]🪄️🔮️2gl📲️]e6.c`, 85 lines) just spawns N
pthreads that each block on one `system()` call - a parallel
batch-launcher, not a cooperating keyboard/VM/renderer triad like
mutaclsym's `button.sh run`.

**Data shape**: entities are directories containing full pro-forma
financial-statement TEXT (Assets/Liabilities/Equity, credit rating,
stock data - e.g. `corporations/generated/ORB/ORB.txt`,
`governments/generated/Inland/financial_profile.txt`), parsed back with
`strstr`/`sscanf` against label text (`financing.c:190-226`,
`game.c:185-196`) rather than clean `key=value` lines. Rich, but
brittle compared to mutaclsym's `piece.pdl`/`state.txt` convention.
Registries DO already exist in a pal-registry-adjacent shape though:
`corporations/36_industries_wsr.txt` (numbered list),
`governments/gov-list_earth.txt`/`generated/gov-list.txt`
(tab-delimited Country/pop/gdp rows) - closer in spirit to
`terrain_types.txt`-style pipe-delimited registries than the state
files are.

**"multiverse/" is aspirational, not implemented.** `setup_multiverse.c`
is a 6-line "Hello, Multiverse!" stub. `multiverse/wussup?.txt` just
holds ordinary single-timeline setup params (num_players,
starting_cash, names). The parallel-timeline idea the name implies
isn't built - it's a design idea to keep for later, not code to port.

**Save/load**: same underlying idea as pal's `cp -r` pattern, but
fragmented. `file_submenu.c` `save_game()` (lines 6-84) `mkdir -p`s a
`saves/<name>/{corporations,governments,players,multiverse,data}`
skeleton, `cp -r`s each live subtree in SEPARATELY (~5 separate calls,
not one whole-tree copy), then zips the result and deletes the
unzipped copy (lines 73-78). `load_game()` (86-165) reverses this -
unzip, then `rm -r`+`cp -r` per subsystem - and signals completion via
a `.load_successful` sentinel file. Same core idea as mutaclsym's save
system, more failure-prone in its current form (many small `system()`
calls plus a zip round-trip instead of one atomic-feeling `cp -r`) -
worth simplifying to the single-whole-tree-copy pattern during the
rewrite, not preserving as-is.

**The one genuinely reusable piece of infrastructure already here**:
`presets/schedule.txt`, read by `clockwise_loop.c` (lines 64-110),
maps time intervals (`1_hour`, `1_day`, `3_months`, `1_year`) to shell
commands to run - e.g. `3_months ./+x/payroll_loop.+x`. This is a real,
working plain-text scheduler → command-dispatch registry. Structurally
this is close to what a formal pal-family "ops registry" could become
- worth keeping the CONCEPT (interval-keyed command dispatch) even
though the mechanism (raw shell command strings) should become real
op-by-name calls once ported.

**No formal op/plugin registry, no shared header** - each file
independently re-declares its own copies of `GameTime`/`Player`/`abuf`/
`get_setting`/`kbhit` etc. (verified duplicated boilerplate between
`game.c` and `payroll_loop.c`). Coincidentally similar to the pal
family's own "no shared headers, duplicate freely" doctrine, but not
because it was designed that way here - just how a single-author CLI
project accretes.

**A real bug worth flagging now, not just at rewrite time**:
`payroll_loop.c` contains ZERO payroll logic. Diffing it against
`game.c` shows it's a near-verbatim copy of the main menu loop (same
structs, same `display_main_menu`, same switch/case UI) - the filename
is the only trace of payroll intent. `presets/schedule.txt` still
schedules it as the quarterly payroll job, so today that job just pops
the main menu instead of running payroll. Not fixed here (out of scope
for a docs pass) - flagged so it isn't mistaken for working code during
the rewrite.


## 2. What "editor-ops rewrite" concretely means here

Bigger than mutaclsym's own compatibility audit (`GRAND-ARCHITECTURE.md`
§3) - mutaclsym was already pal-native and just needed an argv-shape
audit. wsr-pal needs an actual PORT to the pal/prisc+x architecture,
not a light touch-up, before it can be opened in `muchipal-editor` or
share ops with the rest of the family. Rough phased shape (not started,
not scoped in fine detail - this is a "what the work looks like," not a
task list to execute blindly):

1. **Entities become pieces.** Corporations/governments/players become
   physical directories with `piece.pdl` + `state.txt` (clean
   `key=value`, not the pro-forma-text-with-sscanf parsing currently
   used) - same russian-doll physical-directory model mutaclsym's items
   already use. The rich financial-statement DATA is worth keeping;
   the label-text-parsing MECHANISM is not.
2. **Verbs become ops.** `financing.c`/`payroll_loop.c` (once it
   actually has payroll logic)/`dividend_loop.c`/`tax_loop.c`/etc.
   become independent argv-driven binaries in an `ops/` dir, callable
   by name with positional args - matching the `CALL_OP "name" "arg1"
   "arg2"` shape `GRAND-ARCHITECTURE.md` §1/§2 already established for
   editor-block compatibility. `financing.c` and `payroll_loop.c`
   already being separate `+x/` binaries makes this step more
   "reshape the CLI contract" than "invent new boundaries."
3. **`schedule.txt`'s interval scheduler becomes a real pal loop.**
   The `1_hour`/`1_day`/`3_months`/`1_year` cadence concept is good and
   worth keeping - reimplemented as a `pal/main_loop.pal` turn-counter
   pattern (matching mutaclsym's own `end_turn`/`tick_monsters`
   cadence) calling real ops by name instead of raw shell strings.
4. **Save/load simplifies to one whole-tree `cp -r`**, dropping the
   five-separate-calls-plus-zip pattern, matching mutaclsym's Phase 7
   save system exactly (`mutaclsym/dox/01-cdda-architecture.md`).
5. **`multiverse/`'s parallel-timeline idea, if it gets a real
   implementation**, is a genuinely interesting economic-sim mechanic
   worth designing for real at that point - not scoped here, flagged
   as promising.

Not started. This section exists so the shape of the work is known
before it begins, per the same "write the architecture before touching
code" practice already used for the rest of `2.muchi-verse`.


## What wasn't checked

Read in full: `documentation_osx_a0.md`, `main.c`, the pthread
orchestrator, `game.c`, `financing.c`, `payroll_loop.c`,
`clockwise_loop.c`, `file_submenu.c`, `setup_multiverse.c`, plus sample
state/registry files. NOT read: `hour_loop.c`, `select_entity.c`,
`db_search.c`, the `ai/` directory's C files, `setup_corporations.c`/
`setup_corporations_stage2.c`, `setup_governments.c`,
`dividend_loop.c`, `tax_loop.c`, `news_loop.c`, `day_loop.c`,
`incorporation.c`, or the `bak/`/`xs/` ("junk code" per the project's
own doc) directories. A second research pass should read the AI/setup
generation files specifically before the rewrite actually starts - this
doc has enough to plan the shape of the work, not enough to execute it
blind.
