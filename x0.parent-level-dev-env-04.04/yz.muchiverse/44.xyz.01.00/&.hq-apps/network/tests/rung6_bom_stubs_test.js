// rung6_bom_stubs_test.js — history, matchMedia, getComputedStyle, MutationObserver
console.log("history type", typeof history, "length", history.length);
console.log("state before", JSON.stringify(history.state));
history.pushState({page:1},"","/page1");
console.log("state after push", JSON.stringify(history.state), "length", history.length);
history.replaceState({page:2},"","/page2");
console.log("state after replace", JSON.stringify(history.state), "length", history.length);
history.back(); console.log("state after back", JSON.stringify(history.state));
history.forward(); console.log("state after forward", JSON.stringify(history.state));
history.go(1); console.log("state after go(1)", JSON.stringify(history.state));

var mql = matchMedia("(max-width: 600px)");
console.log("matchMedia", typeof mql, mql.matches, mql.media);
mql.addListener(function(){}); mql.removeListener(function(){});
console.log("addListener OK");

var cs = getComputedStyle(document.body);
console.log("getComputedStyle", typeof cs, typeof cs.getPropertyValue, cs.getPropertyValue("display"));
console.log("cs.display", cs.display);

var obs = new MutationObserver(function(){});
obs.observe(document.body,{childList:true});
obs.disconnect();
var r = obs.takeRecords();
console.log("MutationObserver", typeof obs, Array.isArray(r), r.length);

console.log("OK_BOM_STUBS");