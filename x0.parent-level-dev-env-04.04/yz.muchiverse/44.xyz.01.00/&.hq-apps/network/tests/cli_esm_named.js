export var A = 5;
export const B = "bee";
export function add(x, y) {
  return x + y;
}
export { A as renamedA };
console.log("esm-named-loaded");