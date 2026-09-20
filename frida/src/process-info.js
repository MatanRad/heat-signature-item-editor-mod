import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();
console.log(JSON.stringify(hs.info(), null, 2));
