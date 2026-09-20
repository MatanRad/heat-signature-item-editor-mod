import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();
console.log(JSON.stringify(hs.inventory(), null, 2));

rpc.exports = {
  inventory: () => hs.inventory()
};
