# MUTACLSXM

A small CDDA-inspired survival game: explore a tile map, manage hunger/thirst,
fight monsters, craft, and survive. This doc is for *playing* the game - see
`dox/` for the internal architecture if you're developing it instead.

## Starting the game

```
./button.sh compile   # first time only, or after any code change
./button.sh chtpm     # recommended - real menu shell + GL window
```

`./button.sh run` also works (older, plain terminal + optional GL mirror, no
menu chrome). Set `NO_GL=1` before either command to skip the GL window
entirely. `./button.sh kill` stops any lingering game processes.

## Controls

### Normal movement
- **wasd / arrows** - move the hero. Walking into a monster attacks it
  instead of moving (melee, using whatever weapon you're carrying - see
  Combat below).
- **e** - toggle ASCII/emoji display, any time.
- **q** - quit.

### The action bar (top of screen)
Arrows/digits select an option, Enter commits it (or click it directly, if
you're in the chtpm menu shell). Most options fire immediately:
- **pickup** - pick up whatever's on the ground under you.
- **drop** - drop the first item in your inventory.
- **eat** - eat/drink the first food or drink item you're carrying.
- **save** - save your game.
- **toggle_emoji** - same as pressing 'e'.

Two options open their own submenu instead of firing immediately:
- **craft** - pick a recipe to craft from what's in your inventory.
- **examine** - browse your inventory (read-only).

### Interact mode (look around / aim at range)
Select **Control Hero** to enter interact mode: wasd/arrows now move a
free-roaming cursor instead of the hero (it can pass through walls - it's a
look/aim tool, not a second body).

- **Enter** - examine whatever's under the cursor, OR, if the cursor is on
  the hero's own tile, return control to the hero (same as pressing Escape).
- **t** - throw your best carried weapon at whatever's under the cursor, if
  it's a monster within range. The weapon is consumed either way once
  thrown.
- **Escape** - always returns to normal movement.

### Help
There's a real in-game **help** option on the action bar with this same
control summary, in case you forget mid-game.

## Combat

- **Melee** (bump into a monster): damage comes from the best weapon in your
  inventory (higher `power` wins), or bare fists if you're not carrying one.
- **Thrown** (interact mode, `t`): same weapon pool, but at range instead of
  adjacent. The weapon is used up when thrown - it doesn't come back.
- No guns/ammo yet - that's a deliberately separate, bigger feature for
  later (see `dox/01-cdda-architecture.md` §6 if you're curious).

## Crafting

Select **craft** on the action bar, pick a recipe. Recipes show `(ready)` if
you have the materials, `(missing)` if you don't - you can still see what a
recipe needs, you just can't make it yet.

## Saving

Select **save** on the action bar. Loads are done by launching the game
again with that save's world in place - ask a developer if you need the
exact steps, this part's still developer-facing.

## Tips

- Hunger and thirst climb over time - eat/drink before they hit 100
  (starving/dehydrated).
- Monsters chase and attack on their own; you don't have to seek them out.
- Emoji mode is the default, richer look (also required for GL colors) -
  ASCII is the plainer fallback, toggle back any time with 'e'.
