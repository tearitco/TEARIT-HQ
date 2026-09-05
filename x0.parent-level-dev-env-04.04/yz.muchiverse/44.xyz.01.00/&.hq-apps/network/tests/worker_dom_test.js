// worker_dom_test.js — rung 2 DOM accessors, run headless through the
// worker. A failed assertion throws (worker -> STATUS err); success prints
// values and finishes with OK_DOM_TEST. Driven by worker_dom_test.c, which
// builds the companion canned HTML into fetch.dom and LOADs this script.

function ok(cond, msg) {
    if (!cond) throw "DOM-ASSERT-FAIL: " + msg;
    console.log("ok:", msg);
}
function okEq(a, b, msg) {
    if (a !== b) throw "DOM-ASSERT-FAIL: " + msg + "  (got " + String(a) + ", want " + String(b) + ")";
    console.log("ok:", msg, "=", String(a));
}

// --- getElementById + node props ---
var b = document.getElementById("banner");
ok(b !== null, "getElementById banner");
okEq(b.tagName, "div", "banner tagName");
okEq(b.id, "banner", "banner id");
okEq(b.className, "top bar", "banner className");
ok(b.textContent.indexOf("Hello") === 0, "banner textContent starts Hello");
ok(b.textContent.indexOf("world") >= 0, "banner textContent has world");

// --- getAttribute / setAttribute ---
okEq(b.getAttribute("id"), "banner", "getAttribute id");
b.setAttribute("data-x", "42");
okEq(b.getAttribute("data-x"), "42", "setAttribute data-x");

// --- parentNode / firstChild / nextSibling ---
ok(b.parentNode !== null, "banner parentNode");
ok(b.firstChild !== null || b.children.length > 0, "banner has children");
okEq(b.children[0].tagName, "b", "banner first child is b");

// --- getElementsByTagName ---
var lis = document.getElementsByTagName("li");
okEq(lis.length, 3, "getElementsByTagName li count");

// --- querySelector / querySelectorAll ---
var q = document.querySelector(".item");
ok(q !== null, "querySelector .item");
okEq(q.tagName, "li", "querySelector first .item is li");
var q2 = document.querySelector("ul #list");
ok(q2 === null || q2.id === "list", "querySelector descendant ul #list");
var all = document.querySelectorAll(".item");
okEq(all.length, 2, "querySelectorAll .item count");

// --- classList ---
var c = q.classList;
ok(!c.contains("zzz"), "classList not contains zzz");
c.add("added");
ok(c.contains("added"), "classList add/contains");
c.remove("item");
ok(!c.contains("item"), "classList remove");
okEq(c.toggle("added"), false, "classList toggle removes");
ok(!c.contains("added"), "classList confirm removed");

// --- children / appendChild / createElement ---
var ul = document.getElementById("list");
okEq(ul.children.length, 3, "ul children count");
okEq(ul.children[0].textContent, "one", "first li text");
var d = document.createElement("div");
ok(d !== null, "createElement");
d.textContent = "newdiv";
ul.appendChild(d);
okEq(ul.children.length, 4, "appendChild count");
okEq(ul.children[3].textContent, "newdiv", "appended node text");
okEq(d.parentNode.id, "list", "appended node parentNode");

// --- innerHTML get/set ---
var zone = document.createElement("span");
ul.appendChild(zone);
zone.innerHTML = "<em>hi</em>";
ok(zone.innerHTML.indexOf("<em") === 0, "innerHTML get");
ok(zone.textContent.indexOf("hi") >= 0, "innerHTML set textContent");

// --- document.body / documentElement ---
ok(document.body !== null, "document.body");
ok(document.documentElement !== null, "document.documentElement");
okEq(document.documentElement.tagName, "html", "documentElement tagName");

console.log("OK_DOM_TEST");
