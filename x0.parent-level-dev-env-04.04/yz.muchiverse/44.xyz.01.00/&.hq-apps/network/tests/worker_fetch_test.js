// worker_fetch_test.js — rung 4 fetch()/XMLHttpRequest headless test.
// Runs inside the NB-JS worker (via worker_fetch_test.c). __DATA__ is
// substituted by the driver with the absolute file:// data fixture URL.
// The page must render  (fetch then XHR) before RENDER is serialized;
// the driver asserts the joined marker in the TEXT row.
var __st = 0;
fetch("__DATA__").then(function (r) { __st = r.status; return r.text(); }).then(function (t) {
    var el = document.getElementById("r");
    if (!el) { throw new Error("no #r in DOM"); }
    el.textContent = "F" + __st + "-" + t;
    var x = new XMLHttpRequest();
    x.open("GET", "__DATA__");
    x.onload = function () { el.textContent += "|X" + x.status + "-" + x.responseText; };
    x.send();
});