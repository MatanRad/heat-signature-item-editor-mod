"use strict";

// Historical prototype retained for reference. Its RValue kind names and some
// thread-safety assumptions predate frida/src/heat-signature.js. Use the
// compiled toolkit for new investigations.

if (
  Process.platform !== "windows" ||
  Process.arch !== "ia32" ||
  Process.pointerSize !== 4
) {
  throw new Error(
    `Expected 32-bit Windows; got ${Process.platform}/${Process.arch}`
  );
}

const game = Process.getModuleByName("Heat_Signature.exe");
const base = game.base;

const RVA_GLOBAL_CONTAINER = 0x0453d610;
const RVA_VARIABLE_TABLE = 0x0453d5b8;
const RVA_RESOLVE_CINSTANCE = 0x00c98df0;
const RVA_GET_VAR = 0x00c99410;
const RVA_SET_VAR = 0x00c996f0;
const RVA_SET_STRING = 0x00cab130;
const RVA_ARRAY_LOOKUP = 0x00cbbea0;
const RVA_SET_GUN_SPRITES = 0x003f2e00;
const RVA_ASSIGN_AS_PRIMARY_ITEM = 0x004cf140;
const RVA_ASSIGN_AS_SECONDARY_ITEM = 0x004d02a0;

const VAR_GLOBAL_PLAYER = 5;
const VAR_INVENTORY = 624;
const VAR_DESCRIPTION = 626;
const VAR_INVENTORY_COUNT = 627;
const VAR_BASE_NAME = 628;
const VAR_NAME = 673;

const NO_ARRAY_INDEX = -2147483648;
const RVALUE_SIZE = 0x10;
const RVALUE_TYPE_OFFSET = 0x0c;
const CINSTANCE_SPRITE_INDEX_OFFSET = 0x8c;
const MAX_VARIABLE_COUNT = 10000;
const MAX_ARRAY_LENGTH = 4096;
const VK_CONTROL = 0x11;
const VK_LCONTROL = 0xa2;
const VK_RCONTROL = 0xa3;

let variableIds = null;

const getVar = new NativeFunction(
  base.add(RVA_GET_VAR),
  "int",
  ["int", "int", "int", "pointer"],
  "mscdecl"
);

const setVar = new NativeFunction(
  base.add(RVA_SET_VAR),
  "int",
  ["int", "int", "int", "pointer"],
  "mscdecl"
);

const setString = new NativeFunction(
  base.add(RVA_SET_STRING),
  "int",
  ["pointer", "pointer"],
  "mscdecl"
);

const resolveCInstance = new NativeFunction(
  base.add(RVA_RESOLVE_CINSTANCE),
  "pointer",
  ["int"],
  "mscdecl"
);

const arrayLookup = new NativeFunction(
  base.add(RVA_ARRAY_LOOKUP),
  "pointer",
  ["pointer", "int"],
  "mscdecl"
);

const setGunSprites = new NativeFunction(
  base.add(RVA_SET_GUN_SPRITES),
  "pointer",
  ["pointer", "pointer", "pointer", "int", "pointer"],
  "mscdecl"
);

const getAsyncKeyState = new NativeFunction(
  Process.getModuleByName("user32.dll").getExportByName("GetAsyncKeyState"),
  "int16",
  ["int"],
  "stdcall"
);

function clearRValue(rv) {
  rv.writeByteArray([
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0,
    0, 0, 0, 0
  ]);
}

function readRValue(rv) {
  const rawType = rv.add(RVALUE_TYPE_OFFSET).readU32();
  const type = rawType & 0xff;

  switch (type) {
    case 0:
      return {
        type: "real",
        value: rv.readDouble()
      };

    case 1: {
      const yyString = rv.readPointer();
      if (yyString.isNull()) {
        return { type: "string", value: null };
      }

      const text = yyString.readPointer();
      const length = yyString.add(8).readU32();

      if (text.isNull() || length > 4096) {
        return {
          type: "string",
          value: null
        };
      }

      return {
        type: "string",
        value: text.readUtf8String(length)
      };
    }

    case 2:
      return {
        type: "array",
        value: rv.readPointer().toString()
      };

    case 3:
      return {
        type: "pointer",
        value: rv.readPointer().toUInt32()
      };

    case 5:
      return {
        type: "int",
        value: rv.readS32()
      };

    case 6:
      return {
        type: "bool",
        value: rv.readS32() !== 0
      };

    case 7:
      return {
        type: "undefined",
        value: undefined
      };

    case 10:
      return {
        type: "int64",
        value: rv.readU64().toNumber()
      };

    default:
      return {
        type: `unknown:${type}`,
        value: rv.readPointer(),
        rawType
      };
  }
}

function getVariableIds() {
  if (variableIds !== null) {
    return variableIds;
  }

  const variableTable = base.add(RVA_VARIABLE_TABLE).readPointer();
  if (variableTable.isNull()) {
    throw new Error("GameMaker variable table is not ready");
  }

  const count = variableTable.add(4 * Process.pointerSize).readU32();
  const descriptors = variableTable.add(5 * Process.pointerSize).readPointer();

  if (count === 0 || count > MAX_VARIABLE_COUNT || descriptors.isNull()) {
    throw new Error(
      `Invalid GameMaker variable table: count=${count}, descriptors=${descriptors}`
    );
  }

  const ids = new Map();

  for (let id = 0; id < count; ++id) {
    const descriptor = descriptors.add(id * Process.pointerSize).readPointer();
    if (descriptor.isNull()) {
      continue;
    }

    const nameAddress = descriptor.readPointer();
    if (nameAddress.isNull()) {
      continue;
    }

    const name = nameAddress.readUtf8String();
    if (name !== null && name.length !== 0) {
      ids.set(name, id);
    }
  }

  variableIds = ids;
  return variableIds;
}

function getVariableId(name) {
  const id = getVariableIds().get(name);
  if (id === undefined) {
    throw new Error(`Unknown GameMaker variable: ${name}`);
  }

  return id;
}

function getGlobalRValue(variableId) {
  const container = base.add(RVA_GLOBAL_CONTAINER).readPointer();
  if (container.isNull()) {
    return null;
  }

  const slots = container.add(4).readPointer();
  if (!slots.isNull()) {
    return slots.add(variableId * RVALUE_SIZE);
  }

  const vtable = container.readPointer();
  if (vtable.isNull()) {
    throw new Error("GameMaker global container has no vtable");
  }

  const lookupAddress = vtable.add(Process.pointerSize).readPointer();
  if (lookupAddress.isNull()) {
    throw new Error("GameMaker global variable lookup is unavailable");
  }

  const lookup = new NativeFunction(
    lookupAddress,
    "pointer",
    ["pointer", "int"],
    "thiscall"
  );

  const result = lookup(container, variableId);
  return result.isNull() ? null : result;
}

function getCurrentCharacter() {
  const playerRValue = getGlobalRValue(VAR_GLOBAL_PLAYER);
  if (playerRValue === null) {
    return {
      available: false,
      reason: "global.Player is unavailable"
    };
  }

  const player = readRValue(playerRValue);
  if (player.type !== "real" && player.type !== "int") {
    return {
      available: false,
      reason: `global.Player has type ${player.type}`,
      rvalueAddress: playerRValue.toString()
    };
  }

  const handle = Math.trunc(player.value);
  if (!Number.isFinite(handle) || handle <= 0) {
    return {
      available: false,
      reason: "global.Player is unset",
      value: player.value,
      rvalueAddress: playerRValue.toString()
    };
  }

  const instance = resolveCInstance(handle);
  if (instance.isNull()) {
    return {
      available: false,
      reason: "global.Player does not refer to a live instance",
      rawValue: handle,
      rvalueAddress: playerRValue.toString()
    };
  }

  return {
    available: true,
    handle,
    instanceAddress: instance.toString(),
    instanceId: instance.add(0x78).readU32(),
    objectIndex: instance.add(0x7c).readU32(),
    rvalueAddress: playerRValue.toString()
  };
}

function getPropertyRValue(instance, propertyId, arrayIndex = NO_ARRAY_INDEX) {
  const result = Memory.alloc(RVALUE_SIZE);
  clearRValue(result);

  const success = getVar(instance, propertyId, arrayIndex, result);
  return success === 0 ? null : result;
}

function readProperty(instance, propertyId, arrayIndex = NO_ARRAY_INDEX) {
  const result = getPropertyRValue(instance, propertyId, arrayIndex);
  return result === null ? null : readRValue(result);
}

function makeRValue(value, existingType) {
  const result = Memory.alloc(RVALUE_SIZE);
  clearRValue(result);

  if (typeof value === "string") {
    result.add(RVALUE_TYPE_OFFSET).writeU32(1);
    const text = Memory.allocUtf8String(value);
    if (setString(result, text) === 0) {
      throw new Error("GameMaker failed to create a string RValue");
    }
    return result;
  }

  if (typeof value === "boolean") {
    result.writeS32(value ? 1 : 0);
    result.add(RVALUE_TYPE_OFFSET).writeU32(6);
    return result;
  }

  if (typeof value === "number" && Number.isFinite(value)) {
    if (existingType === "pointer") {
      if (!Number.isSafeInteger(value) || value < 0 || value > 0xffffffff) {
        throw new TypeError("Pointer properties require a 32-bit unsigned integer");
      }
      result.writePointer(ptr(value));
      result.add(RVALUE_TYPE_OFFSET).writeU32(3);
    } else if (existingType === "int64") {
      if (!Number.isSafeInteger(value) || value < 0) {
        throw new TypeError("Integer-mask properties require a non-negative safe integer");
      }
      result.writeU64(value);
      result.add(RVALUE_TYPE_OFFSET).writeU32(10);
    } else if (existingType === "int") {
      if (!Number.isInteger(value)) {
        throw new TypeError("Cannot assign a fractional value to an integer property");
      }
      result.writeS32(value);
      result.add(RVALUE_TYPE_OFFSET).writeU32(5);
    } else if (existingType === "bool") {
      result.writeS32(value !== 0 ? 1 : 0);
      result.add(RVALUE_TYPE_OFFSET).writeU32(6);
    } else {
      result.writeDouble(value);
      result.add(RVALUE_TYPE_OFFSET).writeU32(0);
    }
    return result;
  }

  if (value === undefined || value === null) {
    result.add(RVALUE_TYPE_OFFSET).writeU32(7);
    return result;
  }

  throw new TypeError(
    `Unsupported property value type: ${typeof value}`
  );
}

function writeProperty(
  instance,
  propertyId,
  value,
  arrayIndex = NO_ARRAY_INDEX,
  typeOverride = null
) {
  const current = readProperty(instance, propertyId, arrayIndex);
  const existingType =
    typeOverride === null
      ? (current === null ? null : current.type)
      : typeOverride;
  const input = makeRValue(value, existingType);
  const success = setVar(instance, propertyId, arrayIndex, input);

  if (success === 0) {
    throw new Error(
      `GameMaker rejected property ${propertyId} at array index ${arrayIndex}`
    );
  }

  return value;
}

function readStringProperty(instance, propertyId) {
  const property = readProperty(instance, propertyId);

  return property !== null && property.type === "string"
    ? property.value
    : null;
}

function itemName(itemHandle) {
  return (
    readStringProperty(itemHandle, VAR_NAME) ||
    readStringProperty(itemHandle, VAR_BASE_NAME) ||
    readStringProperty(itemHandle, VAR_DESCRIPTION) ||
    "(unnamed)"
  );
}

class InventoryItem {
  constructor(index, handle) {
    this.index = index;
    this.handle = handle;
    this.name = itemName(handle);
  }

  get sprite_index() {
    const instance = resolveCInstance(this.handle);
    if (instance.isNull()) {
      throw new Error(`Item ${this.handle} is no longer a live instance`);
    }

    return instance.add(CINSTANCE_SPRITE_INDEX_OFFSET).readS32();
  }

  set sprite_index(value) {
    if (!Number.isInteger(value) || value < 0) {
      throw new TypeError("sprite_index must be a non-negative integer");
    }

    const instance = resolveCInstance(this.handle);
    if (instance.isNull()) {
      throw new Error(`Item ${this.handle} is no longer a live instance`);
    }

    instance.add(CINSTANCE_SPRITE_INDEX_OFFSET).writeS32(value);
  }

  get(propertyName, arrayIndex = NO_ARRAY_INDEX) {
    if (arrayIndex === NO_ARRAY_INDEX) {
      const property = readProperty(
        this.handle,
        getVariableId(propertyName)
      );

      if (property !== null && property.type === "array") {
        return this.getArray(propertyName);
      }

      return property === null ? undefined : property.value;
    }

    const property = readProperty(
      this.handle,
      getVariableId(propertyName),
      arrayIndex
    );

    return property === null ? undefined : property.value;
  }

  set(propertyName, value, arrayIndex = NO_ARRAY_INDEX) {
    const typeOverride =
      propertyName.endsWith("DamageMask") ? "int64" : null;

    writeProperty(
      this.handle,
      getVariableId(propertyName),
      value,
      arrayIndex,
      typeOverride
    );

    return value;
  }

  getArray(propertyName) {
    const propertyId = getVariableId(propertyName);
    const property = getPropertyRValue(
      this.handle,
      propertyId
    );

    if (property === null) {
      return undefined;
    }

    const type = property.add(RVALUE_TYPE_OFFSET).readU32() & 0xff;
    if (type !== 2) {
      throw new TypeError(
        `${propertyName} is not an array (RValue type ${type})`
      );
    }

    const array = property.readPointer();
    if (array.isNull()) {
      return [];
    }

    const physicalLength = array.readU32();
    const traitCount =
      propertyName === "Traits" ? this.get("TraitCount") : undefined;
    const length =
      Number.isFinite(traitCount) && traitCount >= 0
        ? Math.min(Math.trunc(traitCount), physicalLength)
        : physicalLength;

    if (length > MAX_ARRAY_LENGTH) {
      throw new RangeError(
        `${propertyName} has suspicious array length ${length}`
      );
    }

    const values = [];
    for (let index = 0; index < length; ++index) {
      const element = arrayLookup(property, index);
      values.push(element.isNull() ? undefined : readRValue(element).value);
    }

    const item = this;

    return new Proxy(values, {
      set(target, key, value, receiver) {
        if (typeof key !== "string" || !/^(0|[1-9]\d*)$/.test(key)) {
          if (key === "length" && value !== target.length) {
            throw new TypeError("Resizing GameMaker arrays is not supported");
          }
          return Reflect.set(target, key, value, receiver);
        }

        const index = Number(key);
        if (index >= target.length) {
          throw new RangeError(
            `Array index ${index} is out of range for length ${target.length}`
          );
        }

        writeProperty(item.handle, propertyId, value, index);
        return Reflect.set(target, key, value, receiver);
      }
    });
  }

  properties(propertyNames) {
    if (!Array.isArray(propertyNames)) {
      throw new TypeError("properties() expects an array of property names");
    }

    const result = {};
    for (const propertyName of propertyNames) {
      result[propertyName] = this.get(propertyName);
    }
    return result;
  }

  allProperties(includeTypes = false) {
    const result = {};

    for (const [propertyName, propertyId] of getVariableIds()) {
      const property = readProperty(this.handle, propertyId);

      if (
        property === null ||
        property.type === "undefined" ||
        property.type.startsWith("unknown:")
      ) {
        continue;
      }

      result[propertyName] = includeTypes ? property : property.value;
    }

    return result;
  }

  propertyNames() {
    return Array.from(getVariableIds().keys());
  }

  toJSON() {
    return {
      index: this.index,
      handle: this.handle,
      name: this.name
    };
  }
}

function createInventoryItem(index, handle) {
  const item = new InventoryItem(index, handle);

  return new Proxy(item, {
    get(target, property, receiver) {
      if (typeof property !== "string" || property in target) {
        return Reflect.get(target, property, receiver);
      }

      if (!getVariableIds().has(property)) {
        return undefined;
      }

      return target.get(property);
    },

    set(target, property, value, receiver) {
      if (property === "name") {
        target.set("Name", value);
        target.name = value;
        return true;
      }

      if (property === "sprite_index") {
        Reflect.set(target, property, value, receiver);
        return true;
      }

      if (
        typeof property === "string" &&
        getVariableIds().has(property)
      ) {
        target.set(property, value);
        if (property === "Name") {
          target.name = value;
        }
        return true;
      }

      if (property in target) {
        throw new TypeError(`Wrapper property ${String(property)} is read-only`);
      }

      throw new Error(`Unknown GameMaker variable: ${String(property)}`);
    }
  });
}

function readInventory(playerHandle) {
  const countValue = readProperty(playerHandle, VAR_INVENTORY_COUNT);

  const reportedCount =
    countValue !== null &&
    (countValue.type === "real" || countValue.type === "int")
      ? Math.trunc(countValue.value)
      : null;

  const slots =
    reportedCount !== null && reportedCount >= 0
      ? Math.min(reportedCount, 256)
      : 256;

  const items = [];

  for (let index = 0; index < slots; ++index) {
    const slot = readProperty(playerHandle, VAR_INVENTORY, index);

    if (slot === null) {
      break;
    }

    if (slot.type !== "real") {
      continue;
    }

    const itemHandle = Math.trunc(slot.value);
    if (itemHandle <= 0) {
      continue;
    }

    items.push(createInventoryItem(index, itemHandle));
  }

  return items;
}

function getCurrentInventory() {
  const character = getCurrentCharacter();
  if (!character.available) {
    throw new Error(`No current character: ${character.reason}`);
  }

  return readInventory(character.handle);
}

function isControlDown() {
  return [VK_CONTROL, VK_LCONTROL, VK_RCONTROL].some(
    key => (getAsyncKeyState(key) & 0x8000) !== 0
  );
}

function installInventoryAssignmentHook(rva, action) {
  const address = base.add(rva);
  const original = new NativeFunction(
    address,
    "pointer",
    ["pointer", "pointer", "pointer", "int", "pointer"],
    "mscdecl"
  );

  Interceptor.replace(
    address,
    new NativeCallback(
      function (self, other, result, argc, argv) {
        if (!isControlDown() || argc < 1 || argv.isNull()) {
          return original(self, other, result, argc, argv);
        }

        const itemArgument = argv.readPointer();
        if (itemArgument.isNull()) {
          console.log(`[inventory ctrl-click] ${action}: null item`);
          return result;
        }

        const itemReference = readRValue(itemArgument);
        if (!["real", "int", "int64"].includes(itemReference.type)) {
          console.log(
            `[inventory ctrl-click] ${action}: unexpected item type ` +
            itemReference.type
          );
          return result;
        }

        const handle = Math.trunc(itemReference.value);
        const inventoryIndex = readProperty(
          handle,
          getVariableId("InventoryIndex")
        );
        const output = {
          action,
          index:
            inventoryIndex === null
              ? null
              : Math.trunc(inventoryIndex.value),
          handle,
          name: itemName(handle)
        };

        console.log(`[inventory ctrl-click] ${JSON.stringify(output)}`);
        return result;
      },
      "pointer",
      ["pointer", "pointer", "pointer", "int", "pointer"],
      "mscdecl"
    )
  );
}

installInventoryAssignmentHook(
  RVA_ASSIGN_AS_PRIMARY_ITEM,
  "assign-primary"
);
installInventoryAssignmentHook(
  RVA_ASSIGN_AS_SECONDARY_ITEM,
  "assign-secondary"
);

function replaceGunTraits(gun, selectedTraits) {
  const controlledTraits = new Set([
    "Lethal",
    "Concussive",
    "Loud",
    "Quiet",
    "Silenced",
    "Rapid Fire",
    "Extreme Rapid Fire",
    "Ignores Armour"
  ]);

  const traits = gun.getArray("Traits");
  const nextTraits = traits
    .filter(trait => typeof trait === "string" && !controlledTraits.has(trait))
    .concat(selectedTraits);

  const traitsPropertyId = getVariableId("Traits");
  for (let index = 0; index < nextTraits.length; ++index) {
    writeProperty(gun.handle, traitsPropertyId, nextTraits[index], index);
  }

  gun.TraitCount = nextTraits.length;
  return nextTraits;
}

function renameGun(gun, concussive, loudness, fireMode, armourPiercing) {
  const controlledWords =
    /\b(?:Loud|Quiet|Silenced|Quickfire|Automatic|Concussive|Armou?r-Piercing)\b/gi;
  const baseName = gun.Name
    .replace(controlledWords, " ")
    .replace(/\s+/g, " ")
    .trim();

  const prefixes = [
    loudness[0].toUpperCase() + loudness.slice(1),
    fireMode === "quickfire"
      ? "Quickfire"
      : fireMode === "automatic"
        ? "Automatic"
        : null,
    concussive ? "Concussive" : null,
    armourPiercing ? "Armour-Piercing" : null
  ].filter(value => value !== null);

  gun.Name = prefixes.concat(baseName).join(" ");
  return gun.Name;
}

function setGunParams(
  gun,
  concussive,
  loudness,
  fireMode,
  armourPiercing
) {
  if (
    gun === null ||
    typeof gun !== "object" ||
    !Number.isInteger(gun.handle)
  ) {
    throw new TypeError("gun must be an item returned by inventory()");
  }
  if (typeof concussive !== "boolean") {
    throw new TypeError("concussive must be a boolean");
  }
  if (!["normal", "quickfire", "automatic"].includes(fireMode)) {
    throw new TypeError(
      'fireMode must be "normal", "quickfire", or "automatic"'
    );
  }
  if (typeof armourPiercing !== "boolean") {
    throw new TypeError("armourPiercing must be a boolean");
  }
  if (!["loud", "quiet", "silenced"].includes(loudness)) {
    throw new TypeError('loudness must be "loud", "quiet", or "silenced"');
  }
  if (gun.Type !== "Gun") {
    throw new TypeError(`${gun.Name} is not a gun`);
  }

  gun.WeaponDamageMask =
    (concussive ? 2 : 1) | (armourPiercing ? 4 : 0);
  gun.AmmoType = armourPiercing ? 1 : 0;
  gun.SecondsBetweenUses =
    fireMode === "automatic"
      ? 0.1
      : fireMode === "quickfire"
        ? 0.3
        : 2 / 3;

  if (loudness === "silenced") {
    gun.Noise = 0.05;
    gun.AudibleThroughWalls = 0;
  } else {
    gun.Noise = 0.6;
    gun.AudibleThroughWalls = loudness === "loud" ? 1 : 0;
  }

  if (concussive) {
    if (gun.Capacity < 0) {
      gun.Capacity = 16;
      gun.Uses = 16;
    }
    gun.Rechargeable = 1;
  } else {
    gun.Capacity = -1;
    gun.Rechargeable = 0;
  }

  const loudnessTrait =
    loudness[0].toUpperCase() + loudness.slice(1);
  const selectedTraits = [
    concussive ? "Concussive" : "Lethal",
    loudnessTrait
  ];
  if (fireMode === "quickfire") {
    selectedTraits.push("Rapid Fire");
  } else if (fireMode === "automatic") {
    selectedTraits.push("Extreme Rapid Fire");
  }
  if (armourPiercing) {
    selectedTraits.push("Ignores Armour");
  }

  const traits = replaceGunTraits(gun, selectedTraits);

  const instance = resolveCInstance(gun.handle);
  if (instance.isNull()) {
    throw new Error(`Gun ${gun.handle} is no longer a live instance`);
  }

  const result = Memory.alloc(RVALUE_SIZE);
  clearRValue(result);
  setGunSprites(instance, instance, result, 0, NULL);

  const name = renameGun(
    gun,
    concussive,
    loudness,
    fireMode,
    armourPiercing
  );

  return {
    handle: gun.handle,
    name,
    traits,
    sprite_index: gun.sprite_index,
    weaponDamageMask: gun.WeaponDamageMask,
    ammoType: gun.AmmoType,
    noise: gun.Noise,
    audibleThroughWalls: gun.AudibleThroughWalls,
    fireMode,
    secondsBetweenUses: gun.SecondsBetweenUses,
    capacity: gun.Capacity,
    uses: gun.Uses,
    rechargeable: gun.Rechargeable
  };
}

function getItemProperties(index, includeTypes = false) {
  if (!Number.isInteger(index) || index < 0) {
    throw new TypeError("Item index must be a non-negative integer");
  }

  const items = getCurrentInventory();
  if (index >= items.length) {
    throw new RangeError(
      `Item index ${index} is out of range for ${items.length} inventory items`
    );
  }

  return items[index].allProperties(includeTypes);
}

globalThis.currentCharacter = getCurrentCharacter;
globalThis.inventory = getCurrentInventory;
globalThis.itemProperties = getItemProperties;
globalThis.setGunParams = setGunParams;

rpc.exports = {
  currentcharacter: getCurrentCharacter,
  inventory: getCurrentInventory,
  itemproperties: getItemProperties
};

console.log(`Heat_Signature.exe base: ${base}`);
console.log("Ready. At the Frida prompt, run currentCharacter() or inventory().");