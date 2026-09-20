// worker_fetch_post_test.js — the page's fetch()-based innerTube feed script
// (row 35 wired through the host fetch()/Promise surface, which is how the
// real youtube bundle issues /youtubei/v1/* calls).  __HOSTPORT__ ->
// 127.0.0.1:<port>, __APIKEY__ -> fixture-honored API key.
//
//   1. GET /login           -> Set-Cookie yt-visitor_data + SAPISID + sid
//   2. POST /youtubei/v1/browse via fetch() with headers + JSON body;
//      response.json() must parse to {"legs":"ok"}.
//   3. POST /badsig via fetch() signing the WRONG secret -> fixture 401;
//      prove the Promise rejection path surfaces (AUTH=authfail).
//   4. GET /visitor         -> one jar still reattaches (VIS=vis-ok)
// Renders FETCH=<ok>|J=<legs>|AUTH=<authfail>|SID=<sapisid>|VIS=<vis-ok>.
var origin = 'https://www.youtube.com';
function cook(name) {
    var parts = document.cookie.split(';');
    for (var i = 0; i < parts.length; i++) {
        var kv = parts[i].trim().split('=');
        if (kv[0] === name) return kv[1];
    }
    return '';
}
function signHeaders(sapisid, useBadSecret) {
    var sec = useBadSecret ? 'WRONG_SECRET' : sapisid;
    var ts = Math.floor(Date.now() / 1000);
    return {
        'Content-Type': 'application/json',
        'Origin': origin,
        'Authorization': 'SAPISIDHASH ' + ts + '_' + __nb_sha1(ts + ' ' + sec + ' ' + origin)
    };
}
function inpTube(url, key, body, bad) {
    return fetch(url + '?key=' + key + '&prettyPrint=false', {
        method: 'POST',
        headers: signHeaders(cook('SAPISID'), bad),
        body: body
    });
}
var x = new XMLHttpRequest();
x.open('GET', 'http://__HOSTPORT__/login', true);
x.onload = function () {
    var sapisid = cook('SAPISID');
    var b = '{"context":{"client":{"clientName":"WEB","clientVersion":"2.2026.09.17"}},"browseId":"FEwhat_to_watch"}';
    inpTube('http://__HOSTPORT__/youtubei/v1/browse', '__APIKEY__', b, false)
        .then(function (r) { return r.json(); })
        .then(function (j) {
            // real browse JSON drove out through fetch().json(); now prove the
            // rejection path surfaces on a bad signature (fixture -> 401).
            inpTube('http://__HOSTPORT__/badsig', '__APIKEY__', b, true)
                .then(function () { after('okbad', 'okbad', sapisid); })
                .catch(function () { after(j.legs, 'authfail', sapisid); });
        })
        .catch(function (e) { after('fetchfail:' + e.message, 'x', sapisid); });
};
function after(legs, auth, sapisid) {
    var v = new XMLHttpRequest();
    v.open('GET', 'http://__HOSTPORT__/visitor', true);
    v.onload = function () {
        document.getElementById('r').textContent =
            'FETCH=<ok>|J=<' + legs + '>|AUTH=<' + auth + '>|SID=<' + (sapisid ? sapisid : 'none')
            + '>|VIS=<' + v.responseText + '>';
    };
    v.send();
}
x.send();