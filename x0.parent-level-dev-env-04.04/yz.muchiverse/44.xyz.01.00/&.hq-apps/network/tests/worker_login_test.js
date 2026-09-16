// worker_login_test.js — the HTML page's own script (wall-6 unified cookie
// store). Expanded by worker_login_test.c driver: __HOSTPORT__ -> 127.0.0.1:<port>
// The page makes TWO page-originated XHRs against the loopback fixture server:
//   1. GET /auth  -> server replies Set-Cookie: sid=wlt456; Path=/ (body auth-ok)
//   2. GET /guard -> server replies guard-ok only if the sid=wlt456 cookie came
//      back in the request Cookie header, else guard-fail (401).
// The page reads document.cookie between those two calls and renders
//   SID=<sid>|GUARD=<body>
// proving BOTH halves of the unified store in one round trip: the wire
// Set-Cookie became visible to document.cookie (ingress), and the next
// page-originated request carried it back to the server (egress).
var first = document.cookie;          // empty at start
var x = new XMLHttpRequest();
x.open('GET', 'http://__HOSTPORT__/auth', true);
x.onload = function () {
    // after /auth: the server's Set-Cookie must be in our jar -> document.cookie
    var after = document.cookie;
    var sid = '';
    var parts = after.split(';');
    for (var i = 0; i < parts.length; i++) {
        var kv = parts[i].trim().split('=');
        if (kv[0] === 'sid') { sid = kv[1]; break; }
    }
    var g = new XMLHttpRequest();
    g.open('GET', 'http://__HOSTPORT__/guard', true);
    g.onload = function () {
        document.getElementById('r').textContent =
            'SID=' + (sid ? sid : 'none') + '|GUARD=' + g.responseText;
    };
    g.send();
};
x.send();