/* probe_fingerprint.js - dump every surface Cloudflare's Turnstile/JS
 * challenge reads, as ONE line (the EVAL frame is newline-delimited, so an
 * embedded newline would be split and silently truncate the probe).
 *
 * WHAT THIS ANSWERS
 * -----------------
 * The API-vs-fingerprint question, in two halves:
 *
 *   1. SURFACE: which fingerprint APIs exist at all? A missing one is an
 *      API problem -- the challenge script throws, and the fix is to
 *      implement it.
 *   2. FIDELITY: for the ones that exist, what do they RETURN? A stub that
 *      exists but returns nothing real is a fingerprint problem -- the
 *      script completes and still gets no clearance.
 *
 * A value of "MISSING" in the output is regime 1. A plausible-looking but
 * synthetic value is regime 2, and is the more expensive one.
 *
 * Everything is wrapped: this probe must never itself throw, or it tells us
 * about the probe instead of about the browser.
 */
(function () {
  var out = {};
  function probe(name, fn) {
    try {
      var v = fn();
      out[name] = (v === undefined) ? "undefined"
               : (v === null) ? "null"
               : (typeof v === "string") ? v
               : String(v);
    } catch (e) {
      out[name] = "THROW: " + (e && e.message ? e.message : String(e));
    }
  }
  function has(name, expr) {
    try { out[name] = expr ? "present" : "MISSING"; }
    catch (e) { out[name] = "THROW"; }
  }

  /* ---- 1. SURFACE: does each API exist? ---- */
  has("navigator", typeof navigator !== "undefined" && navigator);
  has("navigator.userAgent", typeof navigator !== "undefined" && navigator.userAgent);
  has("navigator.language", typeof navigator !== "undefined" && navigator.language);
  has("navigator.languages", typeof navigator !== "undefined" && navigator.languages);
  has("navigator.platform", typeof navigator !== "undefined" && navigator.platform);
  has("navigator.hardwareConcurrency", typeof navigator !== "undefined" && navigator.hardwareConcurrency);
  has("navigator.deviceMemory", typeof navigator !== "undefined" && navigator.deviceMemory);
  has("navigator.maxTouchPoints", typeof navigator !== "undefined" && navigator.maxTouchPoints);
  has("navigator.plugins", typeof navigator !== "undefined" && navigator.plugins);
  has("navigator.mimeTypes", typeof navigator !== "undefined" && navigator.mimeTypes);
  has("navigator.webdriver", typeof navigator !== "undefined" && ("webdriver" in navigator));
  has("navigator.onLine", typeof navigator !== "undefined" && navigator.onLine);
  has("navigator.sendBeacon", typeof navigator !== "undefined" && typeof navigator.sendBeacon === "function");
  has("navigator.connection", typeof navigator !== "undefined" && navigator.connection);
  has("screen", typeof screen !== "undefined" && screen);
  has("screen.width", typeof screen !== "undefined" && screen.width);
  has("screen.colorDepth", typeof screen !== "undefined" && screen.colorDepth);
  has("window.innerWidth", typeof innerWidth !== "undefined" && innerWidth);
  has("window.devicePixelRatio", typeof devicePixelRatio !== "undefined" && devicePixelRatio);
  has("window.performance", typeof performance !== "undefined" && performance);
  has("window.PerformanceObserver", typeof PerformanceObserver !== "undefined" && PerformanceObserver);
  has("window.requestAnimationFrame", typeof requestAnimationFrame === "function");
  has("window.Worker", typeof Worker !== "undefined" && Worker);
  has("window.Blob", typeof Blob !== "undefined" && Blob);
  has("window.File", typeof File !== "undefined" && File);
  has("window.FormData", typeof FormData !== "undefined" && FormData);
  has("window.URL", typeof URL !== "undefined" && URL);
  has("window.URL.createObjectURL", typeof URL !== "undefined" && typeof URL.createObjectURL === "function");
  has("window.WebSocket", typeof WebSocket !== "undefined" && WebSocket);
  has("window.RTCPeerConnection", typeof RTCPeerConnection !== "undefined" && RTCPeerConnection);
  has("window.AudioContext", typeof AudioContext !== "undefined" && AudioContext || typeof webkitAudioContext !== "undefined");
  has("window.WebGLRenderingContext", typeof WebGLRenderingContext !== "undefined" && WebGLRenderingContext);
  has("window.OffscreenCanvas", typeof OffscreenCanvas !== "undefined" && OffscreenCanvas);
  has("window.EventSource", typeof EventSource !== "undefined" && EventSource);
  has("window.XMLHttpRequest", typeof XMLHttpRequest !== "undefined" && XMLHttpRequest);
  has("window.fetch", typeof fetch === "function");
  has("window.crypto", typeof crypto !== "undefined" && crypto);
  has("window.crypto.subtle", typeof crypto !== "undefined" && crypto.subtle);
  has("window.crypto.getRandomValues", typeof crypto !== "undefined" && typeof crypto.getRandomValues === "function");
  has("window.localStorage", typeof localStorage !== "undefined" && localStorage);
  has("window.sessionStorage", typeof sessionStorage !== "undefined" && sessionStorage);
  has("window.indexedDB", typeof indexedDB !== "undefined" && indexedDB);
  has("window.MutationObserver", typeof MutationObserver !== "undefined" && MutationObserver);
  has("window.IntersectionObserver", typeof IntersectionObserver !== "undefined" && IntersectionObserver);
  has("window.ResizeObserver", typeof ResizeObserver !== "undefined" && ResizeObserver);
  has("window.Intl", typeof Intl !== "undefined" && Intl);
  has("window.TextEncoder", typeof TextEncoder !== "undefined" && TextEncoder);
  has("window.caches", typeof caches !== "undefined" && caches);
  has("document.createElement('canvas').getContext", (function () {
    try {
      var c = document.createElement("canvas");
      return typeof c.getContext === "function";
    } catch (e) { return false; }
  })());

  /* ---- 2. FIDELITY: what values do the ones that exist actually return? ---- */
  probe("UA", function () { return navigator.userAgent; });
  probe("platform", function () { return navigator.platform; });
  probe("language", function () { return navigator.language; });
  probe("languages", function () { return JSON.stringify(navigator.languages); });
  probe("hardwareConcurrency", function () { return navigator.hardwareConcurrency; });
  probe("deviceMemory", function () { return navigator.deviceMemory; });
  probe("maxTouchPoints", function () { return navigator.maxTouchPoints; });
  probe("webdriver", function () { return String(navigator.webdriver); });
  probe("screen", function () { return screen.width + "x" + screen.height + "@" + screen.colorDepth; });
  probe("availScreen", function () { return screen.availWidth + "x" + screen.availHeight; });
  probe("innerWxH", function () { return innerWidth + "x" + innerHeight; });
  probe("outerWxH", function () { return outerWidth + "x" + outerHeight; });
  probe("dpr", function () { return devicePixelRatio; });
  probe("timezone", function () { return Intl.DateTimeFormat().resolvedOptions().timeZone; });
  probe("tzOffsetMin", function () { return String(new Date().getTimezoneOffset()); });
  probe("plugins.length", function () { return navigator.plugins.length; });
  probe("mimeTypes.length", function () { return navigator.mimeTypes.length; });
  probe("href", function () { return location.href; });

  /* ---- 3. THE COHERENCE FAULT LINES (the expensive regime) ---- */
  probe("perf.now", function () { return performance.now(); });
  probe("perf.timeOrigin", function () { return performance.timeOrigin; });
  probe("perf.timing.navigationStart", function () { return performance.timing.navigationStart; });
  probe("Date.now", function () { return Date.now(); });
  probe("crypto.randomValues16", function () {
    var a = new Uint8Array(16); crypto.getRandomValues(a);
    var s = ""; for (var i = 0; i < 16; i++) s += (a[i] < 16 ? "0" : "") + a[i].toString(16);
    return s;
  });
  probe("canvas.toDataURL", function () {
    var c = document.createElement("canvas");
    c.width = 200; c.height = 50;
    var g = c.getContext("2d");
    if (!g) return "NO_2D_CONTEXT";
    g.textBaseline = "top";
    g.font = "14px 'Arial'";
    g.fillStyle = "#f60";
    g.fillRect(0, 0, 200, 50);
    g.fillStyle = "#069";
    g.fillText("Cloudflare-challenge-probe", 2, 15);
    var url = c.toDataURL();
    return String(url).slice(0, 80) + " len=" + String(url).length;
  });
  probe("canvas.globalCompositeOperation", function () {
    var c = document.createElement("canvas"); c.width = 10; c.height = 10;
    var g = c.getContext("2d");
    if (!g) return "NO_2D_CONTEXT";
    g.globalCompositeOperation = "multiply";
    return g.globalCompositeOperation;
  });
  probe("canvas.measureText.width", function () {
    var c = document.createElement("canvas");
    var g = c.getContext("2d");
    if (!g) return "NO_2D_CONTEXT";
    g.font = "16px monospace";
    return g.measureText("mmmmmmmmmmli").width;
  });
  probe("webgl.vendor", function () {
    var c = document.createElement("canvas");
    var gl = c.getContext("webgl") || c.getContext("experimental-webgl");
    if (!gl) return "NO_WEBGL";
    return String(gl.getParameter(gl.VENDOR)) + " / " + String(gl.getParameter(gl.RENDERER));
  });
  probe("audio", function () {
    var AC = typeof AudioContext !== "undefined" ? AudioContext
             : (typeof webkitAudioContext !== "undefined" ? webkitAudioContext : null);
    if (!AC) return "NO_AUDIOCONTEXT";
    return "present";
  });
  probe("fontProbe", function () {
    /* the classic "does this font render differently from the fallback"
     * test: needs real text metrics to be meaningful */
    var d = document.createElement("div");
    d.style.cssText = "position:absolute;font-size:72px;font-family:monospace";
    d.textContent = "mmmmmmmmmmlli";
    document.body.appendChild(d);
    var w1 = d.getBoundingClientRect().width;
    d.style.fontFamily = "'NoSuchFontXYZ123',monospace";
    var w2 = d.getBoundingClientRect().width;
    return "monospace=" + w1 + " fallback=" + w2;
  });

  /* ---- 4. ELEMENT METHOD SURFACE ----
   * A browser page reaches for these constantly. Reporting them as one
   * group is what turned "TypeError: not a function" (Glassdoor's own
   * timestamp script, at its element.prepend() call) into a named worklist
   * on 2026-11-02. `element.querySelector` being absent while
   * `document.querySelector` works is the surprising one. */
  (function () {
    var el = document.createElement("div");
    var names = ["prepend", "append", "before", "after", "replaceWith",
                 "remove", "matches", "closest", "contains", "cloneNode",
                 "isEqualNode", "normalize", "getClientRects", "scrollIntoView",
                 "insertAdjacentElement", "insertAdjacentHTML", "insertAdjacentText",
                 "replaceChildren", "querySelector", "querySelectorAll",
                 "getElementsByClassName", "getElementsByTagName", "animate"];
    var miss = [];
    for (var i = 0; i < names.length; i++) {
      var v;
      try { v = el[names[i]]; } catch (e) { miss.push(names[i]); continue; }
      if (typeof v !== "function") miss.push(names[i]);
    }
    out["elementMethods.missing"] = miss.length ? miss.join(",") : "(none)";
    /* The same question for document, so a gap can be told apart from a
     * whole-family gap. */
    var dmiss = [];
    var dnames = ["createElement", "createTextNode", "createDocumentFragment",
                  "getElementById", "querySelector", "querySelectorAll",
                  "getElementsByTagName", "getElementsByClassName",
                  "createEvent", "importNode", "adoptNode"];
    for (var j = 0; j < dnames.length; j++) {
      if (typeof document[dnames[j]] !== "function") dmiss.push(dnames[j]);
    }
    out["documentMethods.missing"] = dmiss.length ? dmiss.join(",") : "(none)";
  })();

  /* Emit in CHUNKS, not one JSON blob: the worker's console capture truncates
   * a long line (~500 chars), so a single-line dump silently loses the tail -
   * which is exactly where the interesting values are. */
  var keys = Object.keys(out).sort();
  var buf = "";
  for (var i = 0; i < keys.length; i++) {
    buf += keys[i] + "=" + out[keys[i]] + " ; ";
    if (buf.length > 340) { console.log("PROBE|" + buf); buf = ""; }
  }
  if (buf) console.log("PROBE|" + buf);
  return "PROBE_DONE";
})();
