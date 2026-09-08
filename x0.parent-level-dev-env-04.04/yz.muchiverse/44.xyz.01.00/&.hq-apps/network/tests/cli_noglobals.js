// cli_noglobals.js — node mode must NOT expose browser globals.
var has_window = (typeof window !== "undefined");
var has_document = (typeof document !== "undefined");
console.log(has_window ? "has-window" : "no-window",
            has_document ? "has-document" : "no-document");