# LONG-RANGE SHAPE — the civilization arc

Written 2026-10-01 at the user's direction, while the economy was mid-build. This
is **not** a work plan. It records the destination and the seams that already
exist for it, so that decisions made today do not quietly rule it out.

The short version: this is not a stock simulator that happens to have a menu. It
is a civilization simulation whose economy is one of its systems — and the
trajectory runs from **BC, no tech tree, no governments, no companies**, through
territory and war, to spaceflight.

Everything below is aspirational unless marked **[BUILT]**. Nothing here is
implemented, and no part of it should be read as a promise of sequencing.

---

## 0. The four data domains

The whole game rests on four domains. This is the load-bearing taxonomy, because
it tells you what a new feature is really a feature *of*:

1. **Technology** — what is knowable, and who knows it. **[BUILT, partially]**
2. **Territory & Polity** — who holds land, who governs it, who fights.
3. **Economy** — production, trade, finance, price discovery. **[BUILT]**
4. **Demography & Ecology** — people, households, and later animals.

A "company level" is (1)+(3). A war is (2)+(4). A credit rating is (1)+(3). An
animal is (4). Keeping these separate is what stops the sim becoming one giant
coupled function — each domain gets its own state files and its own ops, and they
communicate only through data contracts, per the existing house rule that ops are
self-contained.

Coercion and civics (§5 — crime, policing, elections, generals) is deliberately
**not** a fifth domain. It cuts across all four rather than sitting beside them:
crime is economy, policing is territory, an election determines who holds
polity, and a jail is demography. Forcing it into its own domain would be a
mistake; it is a *mode of operation* that applies to transfers in the others.

---

## 1. Technology trees — governments and companies both

**[BUILT, partially]** Company tech exists: `ops/goods_sink.c` runs an RPG
level-up curve where breakthrough cost is `250 * 1.6^n`, tracked per corp as
`rd_pool`, `generation`, and a derived `tech_level` label.

**Not built, and the shape that matters:**

- **Government tech trees.** Governments research too, and they research
  *different* things — administration, military doctrine, sanitation, legal
  systems. Government research should not compete with companies for the same
  `rd_pool`.
- **A real tree, not a ladder.** Today a breakthrough is one undifferentiated
  step up. A tree needs prerequisites: you cannot research the transistor before
  you can research vacuum tubes. This is the single biggest change to the tech
  model and it should be a **data file** (`data/tech_tree.txt`), not code, so
  scenarios can ship different trees.
- **Discovery vs. exploitation.** A firm that researches one path hard should not
  be able to research everything. Currently every breakthrough advances the one
  good the firm already produces.
- **BC start.** A BC world has no tech tree *at all* — everything is
  `PROTOTYPE` and the only way forward is grinding the curve. The tier names
  already start at `PROTOTYPE`, which is a lucky fit.

---

## 2. Territory, governments, war

This is the largest unbuilt domain, and `gov_*` pieces already exist as a
skeleton (7 governments, `gov_decide`/`gov_trade` with real fiscal policy).

- **Real estate is already a piece type** (`realestate_*`) — territory is
  partly scaffolded.
- **War must cost something real**, or it is theatre. That means a government
  balance sheet that can fund an army, and a territory model where losing land
  costs you the land's output. Do not add a `at_war=` flag.
- **Treaties** were named in the original design. Same discipline: they should
  move real resources, not just a relationship bit.
- **Sovereignty conflicts with the current entity model.** Governments are
  currently peers of corporations with their own `state.txt`. Territory implies
  governments *contain* pieces. Expect a real refactor here and do it before
  building war on top.

### 2.1 Spatial position — xyz on a planet/country

Explicitly requested, and worth taking seriously because it is load-bearing for
everything above.

A single `x,y` on a flat map is a trap: it silently pretends the world is a
plane and makes distance meaningless (you cannot wrap around a sphere, and you
cannot have two places at once). The shape that will not need throwing away:

- **`x, y, z` are not three flat coordinates.** They are a *hierarchical*
  position: **planet → region → territory → parcel**. That is what lets the same
  model serve a BC village and a space station without a rewrite.
- So: `planet_id`, `region_id`, `x/y` local coordinates **plus an altitude or
  orbit band** — because "space travel" is not a bigger map, it is a different
  *place*, and the domain model must allow a location to be off-world.
- **Distance becomes a real cost**, so logistics (SHIPPING, AIR_FREIGHT) stop
  being decorative industry labels and start being route problems.
- **Resources should be spatially located**, not per-corp. A mine sits somewhere.
  That is what turns BASE_METALS_MINING from a good into a place.

Cost of doing this later instead of now: moderate. Cost of doing it now: low,
because almost nothing depends on coordinates yet. This is the rare case where
the *cheap* move is the early one — but only if it stays a data contract
(`data/locations.txt`) rather than a refactor of every op.

#### 2.2 Zoning — the reason the hierarchy has to be a hierarchy

Government / commercial / residential zoning is the clearest justification for the
tree, and it is worth being explicit about why **flat coordinates cannot express
it** even though they superficially look sufficient.

Zoning is a property of a **parcel**, and a parcel only means something because of
what *contains* it:

- A residential lot **inside** a commercial district is not the same asset as the
  same lot in a rural one. Value comes from access to jobs, to roads, and to
  services — all of which are properties of the surrounding region, not of the
  `(x, y)` pair. With flat `x,y` there is nothing to inherit from, so zoning
  degenerates into a colour-coded flag with no economic content.
- Parcels must be **finite, adjacent and competing**. Contention for land is
  where the auction earns its keep.
- **Land value must be discovered, not computed** — same rule as stock price.
  A parcel's worth is what a bidder will actually pay for it in that location,
  and it should rise near commerce, fall after a war, and collapse when the road
  moves. That means land joins the existing `goods_quote`/`goods_settle` book and
  ledger rather than getting its own pricing formula.

**Terrain is also the natural home of the three sinks** in `goods_sink.c`, which
is a pleasing fit rather than a coincidence:

| sink | on terrain |
|------|------------|
| CONSUMABLE | land is **built on** — a parcel is consumed by the structure on it, and mined out if it holds ore |
| DEPRECIATING | the **building** decays, and the parcel loses value as it does |
| OBSOLESCENT | a location is **made obsolete** by an event — war, a new trade route, a moved industry — and its land is suddenly unsellable at any price |

So war (§2) has a consequence in the goods model rather than being a flag, and a
city that boomed and then emptied is the OBSOLESCENT case made geographic.

**Current state:** `tick_all.ps1` already loops over `realestate_*` pieces, but
**zero exist** — it is a latent no-op today. The type is referenced before it is
real, which is harmless now and will silently misbehave the moment creation lands
without the fields being agreed. Fix the seam before adding the first parcel.

---

## 3. Companies graded — and credit ratings that mean something

Explicitly requested: companies levelled, tied to credit ratings, stats for
companies and people.

**[BUILT, partially]** `generation` and `tech_level` are on every corp.

- **Ratings AAA → CCC should be DERIVED, never stored as an opinion.** A rating
  is a *conclusion about* financial position, so it must be a pure function of
  real fields that already exist: `debt_to_equity`, `cash`, `book_value`,
  margins, and `generation`. If someone can edit a rating, the rating is
  decorative and every downstream use of it (bond pricing, lending) is a lie.
- **This is the same rule as the rest of this project.** Price is discovered, not
  computed; so a rating must be derived, not asserted. The one thing a rating
  legitimately *is* is an opinion — which is exactly why the investor's
  rating should differ from the issuer's, and why a bank marking its own paper
  AAA is a *behaviour worth simulating*.
- **Tech level belongs in the rating.** A firm at generation 5 with a weak balance
  sheet is not AAA; a firm at generation 0 with iron cash might be. That is what
  makes levelling up economically *consequential* rather than cosmetic.

### 3.1 Stats — companies, people, and later animals

The user named stat blocks for companies, people, and eventually animals
(breeding, feeding). Worth stating the generalisation now, before it is built
three times:

- **Stats should be a data-driven block, not per-entity code.** One
  `stats.txt` contract with typed keys, read by whoever cares. A breeding system
  that is statistically similar to an R&D system should reuse the R&D shape.
- **Stats must be disclosed asymmetrically.** Each participant holds a private
  view with noise, exactly like `view_bias()` in `market_quote.c` — that is where
  the game's alpha already lives, and stats are the natural extension. A
  perfectly-known stat block is a solved game.
- Animals are a big honest addition, not a joke: breeding and feeding are the
  same mechanics as production and consumption with a generation time. If the
  sinks generalize, livestock nearly free.

---

## 4. Spaceflight

Named as the far end of the arc. Recording only the constraint that matters:

**Space must be a new PLACE in the domain model, not a larger map.** That is why
§2.1 wants a hierarchical location with an orbit band. If coordinates are flat
`x,y,z` on a plane, space is unimplementable later without a rewrite, and the
rewrite will happen at the worst possible time.

---

## 5. Coercion and civics *(long term — named, not built)*

Named in one breath by the user: crime, law enforcement, jail, elections,
politicians, generals, employees, homelessness. All long term. Recording the
shape, because the list is long and the actual work is short.

### 5.1 The one genuinely new primitive: the INVOLUNTARY transfer

Everything built so far is **voluntary** exchange. Households buy because they
choose to; corps pay wages because they choose to. Crime, confiscation, war
reparations and fines are not that — they move value with **no consent from the
losing side**. That distinction is the whole architectural addition, and it is
*one concept*, not seven systems:

> A ledger row that already exists gains a **cause** and a **coerced** flag.

The double-entry format from `financing.c:256` carries any debit/credit pair
already, so crime is not new machinery — it is a transaction whose cause is
`theft` rather than `sale`. The discipline then applies to everything else here:
if a feature cannot be expressed as a real transfer or a real state change, it is
probably theatre and should be cut.

| feature | what it actually is |
|---------|---------------------|
| **crime** | an involuntary transfer, cause `theft`/`fraud` |
| **law enforcement** | an institution with a budget, like any ministry — but per-region, not per-government |
| **jail** | involuntary removal of a person *from the economy*; and an **income** the state pays (it feeds them, so it touches the §2.2 consumable sink) |
| **elections / politicians** | a mechanism that determines *who occupies a government piece*. Today `gov_*` pieces are autonomous; this is what gives them leadership and makes them losable |
| **generals** | military command — belongs to §2 territory/war, not here |
| **employees** | see below — **mostly already built** |
| **homelessness** | see below — **emergent, needs no new data** |

### 5.2 Employees: a limitation I already documented

`ops/corp_payroll.c` pays wages **equally to every household**, with the source
commenting that "employment does not yet follow demand" as a labelled v1
simplification. Employees are precisely that limitation being removed: an
employee is a household with a job *at a firm*, and the wage should follow what
that firm actually sells them. The B2B note in the `goods_sink.c` header already
anticipates the input (`data/goods_input.txt` states who consumes what).

So employment is **not** a new system — it is generalising an existing, verified
payroll op. Lowest-cost item in this whole document, and the highest leverage on
the goods market, because it makes demand *follow* production rather than lagging
behind it.

### 5.3 Homelessness and jail are emergent — do not build them directly

Both are worth resisting the urge to implement as their own feature, because each
falls out of two others:

- **Homeless** = has no job (§5.2) **and** has no parcel (§2.2). You cannot be
  homeless without there being somewhere to be homeless *from*, and somewhere to
  be housed. Neither homelessness nor housing needs a field: it is the absence of
  two facts that already exist.
- **Jail** = convicted (§5.1 crime) **and** enforcement capacity exceeded
  (§5.1 law enforcement). Jail population is the overflow of the policing budget.

The test to apply when any of this gets built: *can this state be derived from
facts already in the world?* If yes, derive it — a stored `homeless=1` flag
would be a duplicate that can disagree with the truth.

---

## 6. What this costs the current build, and what it does not

Honest accounting, because long-range shape is only worth recording if it does not
distort what is being built now:

- **Nothing above requires changing the economy's design.** The auction, the
  ledger, the sinks, the discovery of price — all survive intact. They are the
  layer that the others are *built on*.
- **The tech tree (§1) is the one item that will touch what already works.** A
  tree with prerequisites is not a refactor of `goods_sink.c`, but it will
  replace its flat curve. Do it as a data change, and do it when there is time —
  not mid-build.
- **Risk of the vision distorting the sim: real.** A civilization game's failure
  mode is a spreadsheet with a theme. The economy being real, conserving money
  and discovering prices is the thing that keeps it honest. Every item above is
  worth adding *only because* that foundation is real; none of them justify
  weakening it.

---

## 7. Near-term order (unchanged by this document)

The vision does not reorder the actual work. These are still the priorities, and
they are still small:

1. Income-sized household bids (households still liquidate opening wealth).
2. Menu items — 21 of 27 are stubs; that is the gap between a sim and a game.
3. Equity `market_quote`/`market_settle` into the turn loop.
4. B2B goods/services, same book, same ledger.

Then the medium ones: real tech tree with prerequisites (§1), government research
(§1), derived credit ratings (§3), spatial locations (§2.1).
