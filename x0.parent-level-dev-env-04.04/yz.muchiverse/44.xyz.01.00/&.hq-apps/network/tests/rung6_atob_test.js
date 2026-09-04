// rung6_atob_test.js — atob/btoa + setTimeout/setInterval stubs
console.log("btoa empty", btoa(""));
console.log("btoa hello", btoa("hello"));
console.log("btoa abc", btoa("abc"));
console.log("atob empty", atob(""));
console.log("atob hello", atob(btoa("hello")));
console.log("atob abc", atob(btoa("abc")));
console.log("atob pads", atob(btoa("ab") + "="));

var t1 = setTimeout(function(){ console.log("SHOULD NOT FIRE"); }, 0);
var t2 = setInterval(function(){ console.log("SHOULD NOT FIRE"); }, 100);
console.log("timer ids", typeof t1, t1 > 0, typeof t2, t2 > 0);
clearTimeout(t1); clearInterval(t2);
console.log("clear did not throw");

console.log("OK_ATOB_TIMER");