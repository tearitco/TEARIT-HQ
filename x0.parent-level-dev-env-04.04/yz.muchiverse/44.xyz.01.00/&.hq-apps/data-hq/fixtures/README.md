# data-hq fixtures: FIXTURES, not game data

Tiny hand-made tables (2026-11-09) that prove `data_op` list, sniff, check, show and stats handle the shapes of games that do not exist yet:
Chutes and Ladders (`chutes_markov`, `chutes_games`: CHUTES plan section 10), Snake (`snake_runs`), Teario (`teario_runs`, `teario_deaths`: TEARIO plan section 6),
plus two generic tables (`weather.tsv`, `scores.pdl`). Every number was made up or hand solved (the Markov table's expected turns 6, 6, 4, 0 are worked out in `chutes_markov.schema.pdl`).
`<name>.schema.pdl` is the schema the sniffer is expected to reproduce (`data_op sniff <table> --against <schema>`). Nothing here is read by any game. Harness case: `data_hq_p0`.
