import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();
hs.watchInventoryClicks();
globalThis.stop = () => hs.stopAll();

console.log("Watching primary and secondary inventory assignment. Run stop() to detach.");
