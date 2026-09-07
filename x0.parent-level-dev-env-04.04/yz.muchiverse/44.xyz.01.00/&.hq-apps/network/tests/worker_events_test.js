// worker_events_test.js — rung 3 events + timers headless test.
// Runs inside the NB-JS worker (via worker_events_test.c). Proves the
// lifecycle fire (DOMContentLoaded/load), the microtask + timer drain loop,
// dispatchEvent (bubbling, preventDefault, stopPropagation), the Event
// constructor, el.on* behavior and el.click(). Every phenomenon appends a
// fixed token to div#log via textContent=; the driver asserts the tokens
// (minus the '|' separators the RENDER serializer turns into spaces) show up
// in the RENDER TEXT row and that a stopped oneshot did not run.
var log = document.getElementById("log");
var btn = document.getElementById("btn");
if (!log || !btn) throw new Error("missing #log/#btn");
function mark(s) { log.textContent += "|" + s; }

mark("A"); // page-script body runs before any lifecycle event

document.addEventListener("DOMContentLoaded", function (e) {
    mark("B-dcl");
    if (!(e && e.type === "DOMContentLoaded")) throw new Error("bad DCL event");
});
window.addEventListener("load", function () { mark("C-load"); });
window.onload = function () { mark("D-winonload"); };

queueMicrotask(function () { mark("E-micro"); });

setTimeout(function () { mark("F-t0"); }, 0);
setTimeout(function () { mark("G-t10"); }, 10);

var ticks = 0, hw = true;
var tid = setInterval(function () {
    if (!hw) return;
    ticks++;
    mark("H-int" + ticks);
    if (ticks >= 3) { clearInterval(tid); hw = false; }
}, 5);

// element events: addEventListener + on* + bubbling to document, click()
btn.addEventListener("click", function (e) {
    mark("I-listener");
    if (!(e && e.type === "click" && e.target.id === "btn" && e.currentTarget.id === "btn"))
        throw new Error("click target/currentTarget");
});
btn.onclick = function () { mark("J-onclick"); };
document.addEventListener("click", function (e) {
    mark("K-bubble");
    if (!(e && e.target.id === "btn")) throw new Error("bubble target");
});
btn.click();

// Event constructor + dispatchEvent with cancelable/preventDefault
var ev = new Event("change", { bubbles: true, cancelable: true });
btn.addEventListener("change", function (e) {
    mark("L-change");
    if (!(e.cancelable === true)) throw new Error("cancelable lost");
});
btn.onchange = function () { mark("M-onchange"); };
btn.dispatchEvent(ev);

var px = new Event("xprevent", { cancelable: true });
btn.addEventListener("xprevent", function (e) { e.preventDefault(); });
btn.addEventListener("xprevent", function (e) {
    mark("N-later");
    if (e.defaultPrevented !== true) throw new Error("preventDefault not seen");
});
btn.dispatchEvent(px);

// stopPropagation on an unbubbling dispatch must keep document listeners out
var nx = new Event("xstop", { bubbles: true });
btn.addEventListener("xstop", function (e) { e.stopPropagation(); });
document.addEventListener("xstop", function () { throw new Error("propagation not stopped"); });
btn.dispatchEvent(nx);
mark("O-stopped");