import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();
console.log(JSON.stringify(hs.currentCharacter(), null, 2));

rpc.exports = {
  current: () => hs.currentCharacter()
};
