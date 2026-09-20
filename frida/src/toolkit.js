import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();

globalThis.hs = hs;
globalThis.currentCharacter = () => hs.currentCharacter();
globalThis.inventory = () => hs.inventory();
globalThis.itemProperties = (index, includeKinds = false) => {
  const item = hs.inventory()[index];
  if (item === undefined) {
    throw new RangeError(`Inventory index ${index} is out of range`);
  }
  return hs.dumpProperties(item.handle, includeKinds);
};

rpc.exports = {
  info: () => hs.info(),
  currentcharacter: () => hs.currentCharacter(),
  inventory: () => hs.inventory(),
  itemproperties: (index, includeKinds = false) =>
    globalThis.itemProperties(index, includeKinds)
};

console.log(
  "Heat Signature toolkit ready: hs, currentCharacter(), inventory(), " +
  "itemProperties(index)"
);
