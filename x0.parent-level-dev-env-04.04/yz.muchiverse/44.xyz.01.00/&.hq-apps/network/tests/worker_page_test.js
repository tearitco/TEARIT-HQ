// worker_page_test.js — the HTML page's own script (page-originated InnerTube
// `next` XHR with session attach). Expanded by driver into page.js:
//   - __NEXT__  -> file:// fixture URL (same substitution worker_fetch_test.js
//                  contracts, proves page-originated dispatch carries the
//                  session scoped to the page host, not the fixture host)
//   - sid cookie pre-seeded in NB_COOKIES_FILE jar for the page host
// The page reads its OWN cookie (document.cookie) to prove the session is
// attached in-page, then issues the page-originated `next` XHR and renders
// "YTNEXT<status>|sid=<v>".
var p = document.cookie;
var sid = '';
var parts = p.split(';');
for (var i = 0; i < parts.length; i++) {
    var kv = parts[i].trim().split('=');
    if (kv[0] === 'sid') { sid = kv[1]; break; }
}
var x = new XMLHttpRequest();
x.open('GET', '__NEXT__', true);
x.onload = function () {
    document.getElementById('r').textContent = 'YTNEXT' + x.status + '|' + (sid ? 'sid=' + sid : 'nosid');
};
x.send();
