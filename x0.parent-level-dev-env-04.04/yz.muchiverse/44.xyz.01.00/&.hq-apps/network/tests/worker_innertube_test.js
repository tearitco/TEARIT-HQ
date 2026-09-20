// worker_innertube_test.js — the page's own in-page InnerTube feed script
// (roadmap row 35).  Expanded by worker_innertube_test.c: __HOSTPORT__ ->
// 127.0.0.1:<port>, __APIKEY__ -> the fixture-honored API key.
//
// Exactly like the real youtube page: read visitor-data + SAPISID from
// document.cookie (our one jar), build a SAPISIDHASH signature with the
// engine-generic __nb_sha1, and POST a browse-shaped InnerTube call through
// the browser's own XHR surface with Content-Type / Origin headers set.
// The fixture accepts only when VISITOR cookie + API key + VALID signature
// + a well-formed JSON browse body all arrive.
//
//   1. GET /login  -> Set-Cookie yt-visitor_data + SAPISID + sid (=jar)
//   2. POST /youtubei/v1/browse?key=<KEY>&prettyPrint=false
//   3. GET /visitor-> cookies must still ride along (one jar, egress)
var origin = 'https://www.youtube.com';
function cook(name) {
    var parts = document.cookie.split(';');
    for (var i = 0; i < parts.length; i++) {
        var kv = parts[i].trim().split('=');
        if (kv[0] === name) return kv[1];
    }
    return '';
}
var x = new XMLHttpRequest();
x.open('GET', 'http://__HOSTPORT__/login', true);
x.onload = function () {
    var visitor = cook('yt-visitor_data');
    var sapisid = cook('SAPISID');
    var ts = Math.floor(Date.now() / 1000);
    var it = new XMLHttpRequest();
    it.open('POST', 'http://__HOSTPORT__/youtubei/v1/browse?key=__APIKEY__&prettyPrint=false', true);
    it.setRequestHeader('Content-Type', 'application/json');
    it.setRequestHeader('X-Goog-Visitor-Id', visitor);
    it.setRequestHeader('Origin', origin);
    it.setRequestHeader('Authorization',
        'SAPISIDHASH ' + ts + '_' + __nb_sha1(ts + ' ' + sapisid + ' ' + origin));
    it.onload = function () {
        var v = new XMLHttpRequest();
        v.open('GET', 'http://__HOSTPORT__/visitor', true);
        v.onload = function () {
            document.getElementById('r').textContent =
                'IT=<' + it.responseText + '>|SID=<' + (sapisid ? sapisid : 'none')
                + '>|VIS=<' + v.responseText + '>';
        };
        v.send();
    };
    it.send('{"context":{"client":{"clientName":"WEB","clientVersion":"2.2026.09.17"}},"browseId":"FEwhat_to_watch"}');
};
x.send();