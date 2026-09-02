# 4chan catalog (2026-09-02, C11)

boards.4chan.org/{board}/catalog HTML is a JS shell. Thread tiles live in
https://a.4cdn.org/{board}/catalog.json (tim, no, sub, com). Thumbs:
https://i.4cdn.org/{board}/{tim}s.jpg

Manager ingest_4chan_catalog() runs on those URLs, still ISO C11, no extra
library. Writes IMG sprite rows + LINK to /{board}/thread/{no}. Cap 24.
