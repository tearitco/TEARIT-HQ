// worker_select_test.js — <select>/<option> semantics, run headless through
// the worker. A failed assertion throws (worker -> STATUS err); success ends
// with OK_SELECT_TEST. Driven by worker_select_test.c, which builds the
// companion canned HTML into fetch.dom and LOADs this script.
//
// This is the regression for a dropdown that was present in the DOM but had
// no behaviour: `select.value` went through the generic el.value getter,
// which answers the SELECT's own `value` attribute — an attribute real pages
// essentially never write — so every read came back "" and the page's
// "is anything selected?" branch never fired.

function ok(cond, msg) {
    if (!cond) throw "SELECT-ASSERT-FAIL: " + msg;
    console.log("ok:", msg);
}
function okEq(a, b, msg) {
    if (a !== b) throw "SELECT-ASSERT-FAIL: " + msg +
        "  (got " + String(a) + ", want " + String(b) + ")";
    console.log("ok:", msg, "=", String(a));
}

try {
    // --- the dropdown nobody could read: no value attr on the <select> ---
    var plain = document.getElementById("plain");
    ok(plain !== null, "getElementById plain");
    okEq(plain.tagName, "select", "plain tagName");
    okEq(plain.value, "a", "plain.value = first option (HTML default)");
    okEq(plain.selectedIndex, 0, "plain.selectedIndex = 0");
    okEq(typeof plain.selectedIndex, "number", "selectedIndex is a number");
    okEq(plain.options.length, 3, "plain.options.length");

    // feature detection: a page may branch on the property existing at all
    ok("selectedIndex" in plain, "'selectedIndex' in select");
    ok(!("selectedIndex" in document.getElementById("ta")), "not on textarea");

    // --- option values: the value attr, else the option's text ---
    var p0 = plain.options[0], p1 = plain.options[1], p2 = plain.options[2];
    okEq(p0.value, "a", "option value attr");
    okEq(p2.value, "gamma", "option with no value attr falls back to text");
    okEq(p1.textContent, "beta", "option textContent");

    // --- a markup-selected option wins over first-option ---
    var pre = document.getElementById("pre");
    okEq(pre.value, "two", "pre.value honours the selected attribute");
    okEq(pre.selectedIndex, 1, "pre.selectedIndex = 1");
    okEq(pre.options[0].selected, false, "unselected option reads false");
    okEq(pre.options[1].selected, true, "selected option reads true");

    // --- assignment selects, and fires change exactly once ---
    var fired = 0, lastTarget = null;
    plain.addEventListener("change", function (e) { fired++; lastTarget = e.target; });

    plain.value = "b";
    okEq(plain.value, "b", "select.value = 'b' reads back");
    okEq(plain.selectedIndex, 1, "selectedIndex followed to 1");
    okEq(p0.selected, false, "previous option deselected");
    okEq(p1.selected, true, "new option selected");
    okEq(fired, 1, "change fired once");

    // selecting the same value again still fires — a real control does
    plain.value = "b";
    okEq(fired, 2, "change fires again on re-assign");

    // onchange property handler must see it too
    var viaProp = 0;
    plain.onchange = function () { viaProp++; };
    plain.value = "gamma";
    okEq(plain.value, "gamma", "text-only option selected by its text");
    okEq(fired, 3, "change fired for text option");
    okEq(viaProp, 1, "onchange property handler ran");
    okEq(lastTarget && lastTarget.value, "gamma", "event.target is the select");

    // --- unmatched value leaves the selection alone (HTML behaviour) ---
    plain.value = "nope";
    okEq(plain.value, "gamma", "unmatched value does not clear the selection");
    okEq(fired, 3, "unmatched value fires nothing");

    // --- selectedIndex setter ---
    plain.selectedIndex = 0;
    okEq(plain.value, "a", "selectedIndex = 0 selects the first option");
    okEq(fired, 4, "selectedIndex setter fired change");

    // --- option.selected drives the parent select ---
    var o = plain.options[2];
    o.selected = true;
    okEq(plain.value, "gamma", "option.selected = true updates select.value");
    okEq(plain.selectedIndex, 2, "selectedIndex followed to 2");
    okEq(fired, 5, "option.selected fired change on the select");

    // --- selecting an already-selected option still reports consistent state ---
    okEq(o.selected, true, "option still selected");

    // --- a select with a value attribute still prefers its selection ---
    // (a value attr on the <select> itself must not shadow the option value)
    var weird = document.getElementById("weird");
    okEq(weird.value, "keep", "option value wins over the select's own value attr");
    okEq(weird.selectedIndex, 0, "weird.selectedIndex = 0");

    console.log("OK_SELECT_TEST");
} catch (e) {
    console.log("SELECT-TEST-FAILED: " + String(e));
}
