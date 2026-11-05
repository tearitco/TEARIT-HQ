# books/shop - the RELEASED shop book (2026-11-04, owner decision)

The owner released ONE caller from quarantine: the editor's Shop tab calls `ops/+x/shop_op.+x` in THIS folder. The lab book `books/ledger-lab/` stays quarantined, and so do the other ops (`survivor_op`, `gen_map_op`, `world_common_op`, `mcl_op`): they only accept a folder with a `QUARANTINE` file and know nothing about `RELEASE`.
- `RELEASE` (first line `shop_op`): `shop_op` accepts this folder, and `item_ledger_op` accepts it only when the env `ITEM_LEDGER_CALLER` is `shop_op` (shop_op sets it). A soft guard so no other op or game gets this folder by accident, not a security boundary.
- `items.pdl`, `shop.pdl`: copies from the lab book (prices, sell < buy, coin `coin.play`). Edit prices here, not in the lab.
- `state/ledger.txt`: the live shop ledger (append-only, hash chained). Seeded by the owner's mint on 2026-11-04: shop stock 20 of each priced item and a till of 500 coins; the actor `player` starts with 50 coins. `item_ledger_op <this folder> check` replays it.
- Release record: lab ledger `check` printed `CHECK OK rows=18 items=22` before the release; harnesses green at that time: shop 41, mcl_bench 49, world_common 38, gen_map 21, survivor_slice 44, survivor_craft 37, item_ledger 39.
- To take it back: delete `RELEASE` (the ops then refuse this folder).
