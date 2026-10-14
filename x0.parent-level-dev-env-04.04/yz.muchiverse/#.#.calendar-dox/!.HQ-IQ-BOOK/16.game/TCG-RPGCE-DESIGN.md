# TCG-rpgce — design (2026-10-14)

Paper only. No window, no binary, no new event command. The next build slice is named at the end and waits until this page is accepted.

Two games the owner named, on one pet house:

- Game Boy Pokemon Red: walk, tall grass, a random battle, catch a monster.
- Pokemon TCG on the GBA, and the same table shape as Magic: a hand, a board, energy or land, attack, prize or life.

The monsters in the party are the four terumon already on page teru-test. The walker is Asa, the actor on the office desk, not Harold. The cards are a deck called TEARIT_TCG. Each terumon carries a collection of those cards.

## What already exists

- The party monsters are the teru-test actors, rows 5–8 in `&.widgits/db-hq/data/actors.pdl`: Ember (`terumon_001_ember`), Glacine (`terumon_002_glacine`), Murmur (`terumon_003_murmur`), Solvent (`terumon_004_solvent`). Each profile is `page:teru-test`. Their face, character, and battler fields are empty. A learning is an event page. Do not add an XP table.
- The walker is Asa, actor 9, nickname `asa`, class Hero, profile `page:office`. Harold is actor 1 and stays on `Actor1`. Asa's face, character, and battler fields are empty too, so the first slice uses the office-desk asa picture, not a new sheet and not Harold.
- Map art we can point at today: 12 OHRRPGCE sheets in `#.NNEST_ASSETS/ohrrpgce-tiles`. Each sheet is 320×200 and slices into 20×20 tiles. Names on disk: grass, forest, swamp, snow, snow forest, fantasy, town, house, cave, castle, insides, vikings. These are map tiles.
- Bar themes (mcl, cdda, brix, and the rest) are `bar_skin` rows. An OHR skin is the same catalog later: three 48×48 strips. Not this page's job.
- Movement, doors, and speech stay the events-hq pages the other toys use. Blueprintz shows those pages. No second quest format.

## The walk

One map. Asa is the person who walks. The four terumon are the party, in the order Ember, Glacine, Murmur, Solvent. The map is painted from the OHR sheets above. Tall grass is one tile id, not a new terrain engine. Stepping on it rolls once. Most steps do nothing. A hit starts a battle page.

A battle is the RPG Maker battle screen we already know how to show: party on one side, one wild terumon on the other. Choices are Fight, Item, Catch, Run. Fight uses the actor's Attack. Catch spends one empty capsule from the bag and, on success, appends that terumon as an actor row the same way Ember was added. Run leaves the grass. No XP grant in the first slice. The learning, if any, is an event page, not a number.

## The cards

Each of those four terumon has a list of card ids. That list is the inventory. Asa does not hold the deck. A card is one `.pdl` row, not a picture baked into C:

`CARD | id | name | kind | cost | text`

kind is creature, energy, or trainer. A creature row also has hp and one attack: `atk_cost`, `atk_dmg`. That is enough for a table. It is not a full Magic stack and not a full Pokemon TCG rulebook.

Play is a second screen on the same toy, opened from the pet's menu, not a second app. You and one opponent. Each has a deck (the pet's collection, shuffled), a hand of up to 7, a row of creatures in play, energy attached, and a prize count of 1 for the first slice. Turn: draw one, play one energy or one creature, attach, attack, pass. An attack subtracts `atk_dmg` from the target hp. At 0 the creature leaves play and the attacker takes one prize. Two prizes wins. The opponent in the first slice plays the top creature of its deck and always attacks if it can.

Cards in the bag and terumon in the party are different lists. A caught terumon is not a card. A later slice may print a card of a caught terumon. The first slice does not.

## What the first build is, after this page is accepted

1. A toy folder in the same shape as df-hq: map from the grass and town OHR sheets, Asa walking, the four teru-test actors as the party, tall grass as a tile, one battle event, catch appends one actor.
2. A `cards.pdl` of about twenty cards and a table screen with the turn above.
3. OHR bar skins and the walkabout sprite copy are later. The window comes after this page, not before.
