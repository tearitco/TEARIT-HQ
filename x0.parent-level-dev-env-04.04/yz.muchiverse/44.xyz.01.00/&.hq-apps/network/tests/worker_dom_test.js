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

// --- rung-2 remainder (A): head, createTextNode, getElementsByClassName,
// removeAttribute, style.*, value — run before the classList block mutates
// the first .item li, so getElementsByClassName still sees both items ---
ok(document.head !== null, "document.head exists");
okEq(document.head.tagName, "head", "document.head tagName");

var tn = document.createTextNode("alpha");
okEq(tn.nodeName, "#text", "text node nodeName");
var host = document.createElement("div");
host.appendChild(tn);
okEq(host.childNodes.length, 1, "childNodes includes a text node");
okEq(host.children.length, 0, "children skips text nodes");
ok(host.textContent.indexOf("alpha") >= 0, "textContent includes text node");
ok(host.innerHTML.indexOf("alpha") >= 0, "innerHTML includes text node");

var items = document.getElementsByClassName("item");
okEq(items.length, 2, "getElementsByClassName .item count");
okEq(items[0].textContent, "one", "getElementsByClassName first item");
okEq(!document.getElementsByClassName("zzz").length, true, "getElementsByClassName miss");

var banner = document.getElementById("banner");
banner.removeAttribute("data-x");
ok(banner.getAttribute("data-x") === null, "removeAttribute data-x");
banner.removeAttribute("class");
okEq(banner.className, "", "removeAttribute class clears className");
banner.classList.add("top");

banner.style.color = "red";
okEq(banner.style.color, "red", "style set/read");
okEq(document.getElementById("banner").style.color, "red", "style persists across re-fetch");

var field = document.getElementById("field");
okEq(field.value, "abc", "input default value from attribute");
field.value = "xyz";
okEq(field.value, "xyz", "input value set");
okEq(document.getElementById("field").value, "xyz", "input value persists via wrapper");
okEq(document.getElementById("ta").value, "", "textarea default empty");

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

// --- rung-2 remainder (B): tree mutators — require the appends above —---
var ul = document.getElementById("list");
var beforeN = ul.children.length;
var tmp = ul.children[beforeN - 1];
var ret = ul.removeChild(tmp);
ok(ret === tmp, "removeChild returns the removed node");
okEq(ul.children.length, beforeN - 1, "removeChild shrinks children");
ok(tmp.parentNode === null, "removed node parentNode null");
document.getElementById("banner").appendChild(tmp);
okEq(tmp.parentNode.id, "banner", "removed node re-appends elsewhere");

var ins = document.createElement("li");
ins.textContent = "inserted";
ul.insertBefore(ins, ul.children[0]);
ok(ul.children[0] === ins, "insertBefore places node first");

var thediv = null;
for (var k = 0; k < ul.children.length; k++) if (ul.children[k].tagName === "div") thediv = ul.children[k];
var sub = document.createElement("span");
sub.textContent = "replacement";
var oldret = ul.replaceChild(sub, thediv);
ok(oldret === thediv, "replaceChild returns the old node");
ok(thediv.parentNode === null, "replaced node detached");
okEq(ul.children[ul.children.length - 1].textContent, "replacement", "replaceChild swaps in");
ok(sub.parentNode === ul, "new child parented");

console.log("OK_DOM_TEST");
