# Wraith Browser Assumptions

- This is the first TPMOS hybrid browser host inside Wraith.
- It is not a general web engine yet.
- Phase 1 proves page loading, HTML-ish parsing, DOM receipts, layout receipts, and Wraith rendering.
- Phase 2 proves controlled JavaScript through a separate reusable op, not by hiding script logic inside the UI host.
- Current browser pages are local fixtures so the parser, render path, and JS seam can be validated deterministically.
- Remote fetch, CSS expansion, and broader JS compatibility should grow as shared TPMOS browser ops later rather than becoming project-local drift.
