# nbjs — standalone NB-JS engine CLI (no khtpm/chtpm/GUI dependency).
#
#   make            -> ./nbjs
#   make install    -> $(PREFIX)/bin/nbjs  (default PREFIX=/usr/local)
#   make check      -> build + run the five worker suites + cli_test
#
# To run:   ./nbjs file.js [args...]              # node mode (no DOM) — like node,
#                                                 #   + require()/require('fs') + ESM
#           ./nbjs --browser page.js [fetch.dom]  # DOM page runner + render-back
#           ./nbjs -i                             # interactive REPL, even when piped
#           ./nbjs                                # tty: REPL / pipes: daemon RPC
# Exit codes (both CLI modes): 0 ok / 1 js error / 2 usage. The daemon RPC
# mode (manager back-end) is the same binary with no arguments.

CC      ?= cc
CFLAGS  ?= -std=c11 -Wall -O2
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
JS_DIR  ?= ../js

SRC := ops/nb_js_worker.c nb_dom.c

nbjs: $(SRC) ops/nb_host.h nb_dom.h $(JS_DIR)/duktape.c
	$(CC) $(CFLAGS) -I$(JS_DIR) -o $@ $(SRC) $(JS_DIR)/duktape.c -lm

# headless test drivers (same framing the manager speaks over sockets)
nbw: nbjs
	cp nbjs nbw

wdt: tests/worker_dom_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_dom_test.c nb_dom.c

wft: tests/worker_fetch_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_fetch_test.c nb_dom.c

wet: tests/worker_events_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_events_test.c nb_dom.c

wck: tests/worker_cookie_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_cookie_test.c nb_dom.c

wcn: tests/worker_nav_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_nav_test.c nb_dom.c

wcl: tests/cli_test.c
	$(CC) $(CFLAGS) -o $@ tests/cli_test.c

check: nbjs wdt wft wet wck wcn wcl
	rm -rf /tmp/nbjs-check ; mkdir -p /tmp/nbjs-check
	./wdt ./nbjs tests/worker_dom_test.js /tmp/nbjs-check
	./wft ./nbjs tests/worker_fetch_test.js /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wet ./nbjs tests/worker_events_test.js /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wck ./nbjs /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wcn ./nbjs /tmp/nbjs-check
	./wcl ./nbjs tests /tmp/nbjs-check

install: nbjs
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 nbjs $(DESTDIR)$(BINDIR)/nbjs

clean:
	rm -f nbjs nbw wdt wft wet wck wcn wcl

.PHONY: check install clean