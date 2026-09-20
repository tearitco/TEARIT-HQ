# nbjs — standalone NB-JS engine CLI (no khtpm/chtpm/GUI dependency).
#
#   make            -> ./nbjs
#   make install    -> $(PREFIX)/bin/nbjs  (default PREFIX=/usr/local)
#   make check      -> build + run the six worker suites + cli_test
#
# To run:   ./nbjs file.js [args...]              # node mode (no DOM) — like node,
#                                                 #   + require()/require('fs') + ESM
#           ./nbjs --browser page.js [fetch.dom]  # DOM page runner + render-back
#           ./nbjs -i                             # interactive REPL, even when piped
#           ./nbjs                                # tty: REPL / pipes: daemon RPC
# Exit codes (both CLI modes): 0 ok / 1 js error / 2 usage. The daemon RPC
# mode (manager back-end) is the same binary with no arguments.

CC      ?= cc
CFLAGS  ?= -std=gnu11 -Wall -O2
QWFLAGS := -D_GNU_SOURCE -DCONFIG_VERSION=\"2026-06-04\" -fwrapv
PREFIX  ?= /usr/local
BINDIR  ?= $(PREFIX)/bin
JS_DIR  ?= ../js

SRC := ops/nb_js_worker.c nb_dom.c nb_css.c
# QuickJS 2026-06-04 engine (replaces Duktape 2.7.0 for ES2020+ page code).
QJS := $(JS_DIR)/quickjs.c $(JS_DIR)/cutils.c $(JS_DIR)/libregexp.c \
       $(JS_DIR)/libunicode.c $(JS_DIR)/dtoa.c

nbjs: $(SRC) ops/nb_host.h nb_dom.h $(QJS) $(JS_DIR)/quickjs.h
	$(CC) $(CFLAGS) $(QWFLAGS) -pthread -I$(JS_DIR) -o $@ $(SRC) $(QJS) -lm

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

wst: tests/worker_storage_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_storage_test.c nb_dom.c

wps: tests/worker_scriptseq_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_scriptseq_test.c nb_dom.c

wcs: tests/worker_css_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_css_test.c nb_dom.c

wcl: tests/cli_test.c
	$(CC) $(CFLAGS) -o $@ tests/cli_test.c

wpt: tests/worker_page_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_page_test.c nb_dom.c

wlt: tests/worker_login_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_login_test.c nb_dom.c

wss: tests/worker_sapisid_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_sapisid_test.c nb_dom.c

wit: tests/worker_innertube_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_innertube_test.c nb_dom.c

wfp: tests/worker_fetch_post_test.c nb_dom.c
	$(CC) $(CFLAGS) -o $@ tests/worker_fetch_post_test.c nb_dom.c

check: nbjs wdt wft wet wck wcn wst wps wcs wcl wpt wlt wss wit wfp
	rm -rf /tmp/nbjs-check ; mkdir -p /tmp/nbjs-check
	./wdt ./nbjs tests/worker_dom_test.js /tmp/nbjs-check
	./wft ./nbjs tests/worker_fetch_test.js /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wet ./nbjs tests/worker_events_test.js /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wck ./nbjs /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wcn ./nbjs /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wst ./nbjs /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wps ./nbjs /tmp/nbjs-check
	NBW_CONSOLE=/tmp/nbjs-check/console.out ./wcs ./nbjs /tmp/nbjs-check
	./wpt ./nbjs tests/worker_page_test.js /tmp/nbjs-check
	./wlt ./nbjs tests/worker_login_test.js /tmp/nbjs-check
	./wss ./nbjs tests/worker_sapisid_test.js /tmp/nbjs-check
	./wit ./nbjs tests/worker_innertube_test.js /tmp/nbjs-check
	./wfp ./nbjs tests/worker_fetch_post_test.js /tmp/nbjs-check
	./wcl ./nbjs tests /tmp/nbjs-check

install: nbjs
	install -d $(DESTDIR)$(BINDIR)
	install -m 0755 nbjs $(DESTDIR)$(BINDIR)/nbjs

clean:
	rm -f nbjs nbw wdt wft wet wck wcn wst wps wcs wcl wpt wlt wss wit wfp

.PHONY: check install clean