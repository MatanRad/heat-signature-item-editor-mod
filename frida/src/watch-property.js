import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();

globalThis.watchProperty = (handle, name, intervalMs = 250) =>
  hs.watchProperty(handle, name, intervalMs);
globalThis.stop = () => hs.stopAll();

console.log(
  "Run watchProperty(instanceHandle, 'PropertyName', intervalMs). " +
  "Run stop() to clear all watchers."
);
