// G1.1 fixture: the page's own script (2026-10-13).
//
// The DOM arrives separately (timer.html, via the fetch.dom slot), so
// this file is behaviour only. Kept as its own file because that is how
// the worker is fed: page.js is the page's scripts, fetch.dom is what the
// fetcher saved.

var n = 0;

function bump() {
  n = n + 1;
  var el = document.getElementById("t");
  if (el) { el.textContent = "tick " + n; }
}

setInterval(bump, 500);
bump();