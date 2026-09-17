// worker_sapisid_test.js — the page's own login script (row-34b real login).
// Expanded by worker_sapisid_test.c driver: __HOSTPORT__ -> 127.0.0.1:<port>.
// This is the google-shaped handshake done entirely in ordinary page JS.
// The engine supplies a single generic primitive, __nb_sha1, which returns
// base64(sha1(str)); everything else is stock browser JS any page has.
//
//   1. GET /login    -> fixture grants Set-Cookie: SAPISID=<secret>; sid=ssr77
//   2. GET /guard    -> page reads document.cookie, and (exactly like the real
//                       Google page) computes
//                         ts    = floor(Date.now()/1000)
//                         hash  = __nb_sha1(ts + ' ' + SAPISID + ' ' + origin)
//                         auth  = 'SAPISIDHASH ' + ts + '_' + hash
//                       sends it as Authorization: (= bearer token) plus
//                       Origin: on a /youtubei/v1-style path.
//                       Server independently recomputes the sha1 and replies
//                       guard-ok only on an exact match.
//   3. GET /reagent   -> the step-1 session cookie must still ride along (one
//                        jar; egress on every subsequent request).
// Renders  SIGN=<body>|SID=<sapisid>|RE=<body>  for the driver to assert.
var origin = 'https://www.youtube.com';   // page's own origin, site JS's business
var first = document.cookie;              // empty at start
var x = new XMLHttpRequest();
x.open('GET', 'http://__HOSTPORT__/login', true);
x.onload = function () {
    // after /login: the granted SAPISID + session cookie sit in OUR jar
    var cookie = document.cookie;
    var sapisid = '';
    var parts = cookie.split(';');
    for (var i = 0; i < parts.length; i++) {
        var kv = parts[i].trim().split('=');
        if (kv[0] === 'SAPISID') { sapisid = kv[1]; break; }
    }
    var ts = Math.floor(Date.now() / 1000);
    var g = new XMLHttpRequest();
    g.open('GET', 'http://__HOSTPORT__/guard?key=AIzaZyYOUTUBE', true);
    g.setRequestHeader('Authorization',
        'SAPISIDHASH ' + ts + '_' + __nb_sha1(ts + ' ' + sapisid + ' ' + origin));
    g.setRequestHeader('Origin', origin);
    g.onload = function () {
        var r = new XMLHttpRequest();
        r.open('GET', 'http://__HOSTPORT__/reagent', true);
        r.onload = function () {
            document.getElementById('r').textContent =
                'SIGN=<' + g.responseText + '>|SID=<' + (sapisid ? sapisid : 'none')
                + '>|RE=<' + r.responseText + '>';
        };
        r.send();
    };
    g.send();
};
x.send();