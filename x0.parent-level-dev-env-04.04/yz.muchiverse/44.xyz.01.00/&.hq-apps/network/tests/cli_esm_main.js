import { renamedA as A2 } from "./cli_esm_named.js";
import { add } from "./cli_esm_named.js";
import scale from "./cli_esm_default.js";
console.log("esm-main A2=" + A2 + " add=" + add(2, 3) + " scale=" + scale(4) + " marker=esm-main-ok");