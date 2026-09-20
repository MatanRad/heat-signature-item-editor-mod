import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();

globalThis.watchScript = (rva, name) => hs.watchScript(rva, name);
globalThis.stop = () => hs.stopAll();

console.log(
  "Run watchScript(0xRVA, 'gml_Script_Name'). " +
  "Use an RVA from ScriptFunctions.txt, not an absolute address."
);
