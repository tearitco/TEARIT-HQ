// rung6_cookie_test.js — document.cookie empty-jar stub
console.log("cookie read", JSON.stringify(document.cookie));
document.cookie = "session=abc123; path=/";
console.log("cookie after write", JSON.stringify(document.cookie));
console.log("cookie readonly prop", typeof document.cookie);
console.log("cookie indexOf", document.cookie.indexOf("session"));

console.log("OK_COOKIE");