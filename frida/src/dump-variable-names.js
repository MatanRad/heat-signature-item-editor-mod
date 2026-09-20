import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();
const names = hs.variableNames();
console.log(names.join("\n"));

rpc.exports = {
  names: () => names
};
