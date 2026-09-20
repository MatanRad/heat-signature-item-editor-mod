import { HeatSignatureRuntime } from "./heat-signature.js";

const hs = new HeatSignatureRuntime();
const defaultIndex = 0;

function dump(index = defaultIndex, includeKinds = true) {
  const item = hs.inventory()[index];
  if (item === undefined) {
    throw new RangeError(`Inventory index ${index} is out of range`);
  }
  const properties = hs.dumpProperties(item.handle, includeKinds);
  console.log(JSON.stringify({ item, properties }, null, 2));
  return properties;
}

globalThis.dumpItemProperties = dump;
rpc.exports = { dump };

console.log("Run dumpItemProperties(index, includeKinds) to inspect an item.");
