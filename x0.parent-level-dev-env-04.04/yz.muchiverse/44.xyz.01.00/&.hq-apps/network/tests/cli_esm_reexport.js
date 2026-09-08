export var Q = 99;
export { Q as Q2 };
export * from "./cli_esm_named.js";
console.log("esm-reexport Q=" + Q);