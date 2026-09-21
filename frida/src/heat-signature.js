"use strict";

export const Rva = Object.freeze({
  globalContainer: 0x0453d610,
  variableTable: 0x0453d5b8,
  resolveInstance: 0x00cbc420,
  resolveCInstance: 0x00c98df0,
  getVar: 0x00c99410,
  setVar: 0x00c996f0,
  setString: 0x00cab130,
  assignAsPrimaryItem: 0x004cf140,
  assignAsSecondaryItem: 0x004d02a0
});

export const RValueKind = Object.freeze({
  real: 0,
  string: 1,
  array: 2,
  pointer: 3,
  undefined: 5,
  int32: 7,
  int64: 10,
  doubleBacked13: 13
});

export const VariableId = Object.freeze({
  globalPlayer: 5,
  inventory: 624,
  description: 626,
  inventoryCount: 627,
  baseName: 628,
  name: 673
});

const RVALUE_SIZE = 0x10;
const RVALUE_KIND_OFFSET = 0x0c;
const CINSTANCE_ID_OFFSET = 0x78;
const CINSTANCE_OBJECT_INDEX_OFFSET = 0x7c;
const CINSTANCE_SPRITE_INDEX_OFFSET = 0x8c;
const NO_ARRAY_INDEX = -2147483648;
const MAX_VARIABLE_COUNT = 10000;
const MAX_ARRAY_LENGTH = 4096;
const KIND_MASK = 0x00ffffff;

function requireReadable(address, label) {
  if (address.isNull()) {
    throw new Error(`${label} is null or unreadable: ${address}`);
  }

  try {
    address.readU8();
  } catch (error) {
    throw new Error(`${label} is null or unreadable: ${address}: ${error}`);
  }
}

function clearRValue(value) {
  value.writeByteArray(new Uint8Array(RVALUE_SIZE));
}

export class HeatSignatureRuntime {
  constructor() {
    if (
      Process.platform !== "windows" ||
      Process.arch !== "ia32" ||
      Process.pointerSize !== 4
    ) {
      throw new Error(
        `Expected 32-bit Windows; got ${Process.platform}/${Process.arch} ` +
        `with ${Process.pointerSize}-byte pointers`
      );
    }

    this.game = Process.getModuleByName("Heat_Signature.exe");
    this.base = this.game.base;
    this.variableIds = null;
    this.listeners = new Set();
    this.timers = new Set();

    this.resolveInstanceNative = new NativeFunction(
      this.address(Rva.resolveInstance),
      "int",
      ["pointer"],
      "mscdecl"
    );
    this.resolveCInstanceNative = new NativeFunction(
      this.address(Rva.resolveCInstance),
      "pointer",
      ["int"],
      "mscdecl"
    );
    this.getVarNative = new NativeFunction(
      this.address(Rva.getVar),
      "int",
      ["int", "int", "int", "pointer"],
      "mscdecl"
    );
    this.setVarNative = new NativeFunction(
      this.address(Rva.setVar),
      "int",
      ["int", "int", "int", "pointer"],
      "mscdecl"
    );
    this.setStringNative = new NativeFunction(
      this.address(Rva.setString),
      "int",
      ["pointer", "pointer"],
      "mscdecl"
    );
  }

  address(rva) {
    return this.base.add(rva);
  }

  executableAddress(rva) {
    const address = this.address(rva);
    const range = Process.findRangeByAddress(address);
    if (range === null || !range.protection.includes("x")) {
      throw new Error(`RVA 0x${rva.toString(16)} is not executable`);
    }
    return address;
  }

  info() {
    return {
      platform: Process.platform,
      architecture: Process.arch,
      pointerSize: Process.pointerSize,
      gameBase: this.base.toString(),
      gameSize: this.game.size,
      modules: Process.enumerateModules()
        .filter(module =>
          /^(Heat_Signature\.exe|d3d9\.dll|ModLoader\.dll)$/i.test(module.name)
        )
        .map(module => ({
          name: module.name,
          base: module.base.toString(),
          size: module.size,
          path: module.path
        }))
    };
  }

  decodeRValue(address) {
    requireReadable(address, "RValue");
    const rawType = address.add(RVALUE_KIND_OFFSET).readU32();
    const kind = rawType & KIND_MASK;

    switch (kind) {
      case RValueKind.real:
        return { kind: "real", value: address.readDouble(), rawType };

      case RValueKind.string: {
        const stringObject = address.readPointer();
        if (stringObject.isNull()) {
          return { kind: "string", value: null, rawType };
        }
        requireReadable(stringObject, "YYString");
        const text = stringObject.readPointer();
        const length = stringObject.add(8).readU32();
        if (text.isNull() || length > 4096) {
          return { kind: "string", value: null, rawType };
        }
        return {
          kind: "string",
          value: text.readUtf8String(length),
          rawType
        };
      }

      case RValueKind.array:
        return {
          kind: "array",
          value: address.readPointer().toString(),
          rawType
        };

      case RValueKind.pointer:
        return {
          kind: "pointer",
          value: address.readPointer().toString(),
          rawType
        };

      case RValueKind.undefined:
        return { kind: "undefined", value: undefined, rawType };

      case RValueKind.int32:
        return { kind: "int32", value: address.readS32(), rawType };

      case RValueKind.int64:
        return { kind: "int64", value: address.readS64().toNumber(), rawType };

      case RValueKind.doubleBacked13:
        return {
          kind: "double-backed-13",
          value: address.readDouble(),
          rawType
        };

      default:
        return {
          kind: `unknown:${kind}`,
          value: address.readPointer().toString(),
          rawType
        };
    }
  }

  makeRValue(value, preferredKind = null) {
    const result = Memory.alloc(RVALUE_SIZE);
    clearRValue(result);

    if (typeof value === "string") {
      const text = Memory.allocUtf8String(value);
      if (this.setStringNative(result, text) === 0) {
        throw new Error("GameMaker failed to create a string RValue");
      }
      return result;
    }

    if (typeof value === "boolean") {
      result.writeDouble(value ? 1 : 0);
      result.add(RVALUE_KIND_OFFSET).writeU32(
        preferredKind === RValueKind.doubleBacked13
          ? RValueKind.doubleBacked13
          : RValueKind.real
      );
      return result;
    }

    if (typeof value === "number" && Number.isFinite(value)) {
      if (preferredKind === RValueKind.int64) {
        if (!Number.isSafeInteger(value)) {
          throw new TypeError("INT64 values must be safe JavaScript integers");
        }
        result.writeS64(value);
        result.add(RVALUE_KIND_OFFSET).writeU32(RValueKind.int64);
      } else if (preferredKind === RValueKind.int32) {
        if (!Number.isInteger(value)) {
          throw new TypeError("INT32 values must be integers");
        }
        result.writeS32(value);
        result.add(RVALUE_KIND_OFFSET).writeU32(RValueKind.int32);
      } else {
        result.writeDouble(value);
        result.add(RVALUE_KIND_OFFSET).writeU32(
          preferredKind === RValueKind.doubleBacked13
            ? RValueKind.doubleBacked13
            : RValueKind.real
        );
      }
      return result;
    }

    throw new TypeError(`Unsupported RValue input: ${typeof value}`);
  }

  getVariableIds() {
    if (this.variableIds !== null) {
      return this.variableIds;
    }

    const table = this.address(Rva.variableTable).readPointer();
    requireReadable(table, "variable table");
    const count = table.add(4 * Process.pointerSize).readU32();
    const descriptors = table.add(5 * Process.pointerSize).readPointer();

    if (count === 0 || count > MAX_VARIABLE_COUNT) {
      throw new Error(`Suspicious variable count: ${count}`);
    }
    requireReadable(descriptors, "variable descriptors");

    const ids = new Map();
    for (let id = 0; id < count; ++id) {
      const descriptor = descriptors
        .add(id * Process.pointerSize)
        .readPointer();
      if (descriptor.isNull()) {
        continue;
      }
      const nameAddress = descriptor.readPointer();
      if (nameAddress.isNull()) {
        continue;
      }
      const name = nameAddress.readUtf8String();
      if (name) {
        ids.set(name, id);
      }
    }

    this.variableIds = ids;
    return ids;
  }

  variableNames() {
    return Array.from(this.getVariableIds().keys());
  }

  variableId(name) {
    const id = this.getVariableIds().get(name);
    if (id === undefined) {
      throw new Error(`Unknown GameMaker variable: ${name}`);
    }
    return id;
  }

  globalRValue(variableId) {
    const container = this.address(Rva.globalContainer).readPointer();
    requireReadable(container, "global variable container");

    const slots = container.add(4).readPointer();
    if (!slots.isNull()) {
      return slots.add(variableId * RVALUE_SIZE);
    }

    const vtable = container.readPointer();
    requireReadable(vtable, "global variable container vtable");
    const lookupAddress = vtable.add(Process.pointerSize).readPointer();
    const lookup = new NativeFunction(
      lookupAddress,
      "pointer",
      ["pointer", "int"],
      "thiscall"
    );
    const result = lookup(container, variableId);
    return result.isNull() ? null : result;
  }

  currentCharacter() {
    const valueAddress = this.globalRValue(VariableId.globalPlayer);
    if (valueAddress === null) {
      return { available: false, reason: "global.Player is unavailable" };
    }

    const value = this.decodeRValue(valueAddress);
    if (!["real", "int32", "int64"].includes(value.kind)) {
      return {
        available: false,
        reason: `global.Player has kind ${value.kind}`,
        rawType: value.rawType
      };
    }

    const handle = Math.trunc(value.value);
    const instance = this.resolveCInstance(handle);
    if (instance === null) {
      return {
        available: false,
        reason: "global.Player does not refer to a live instance",
        rawValue: handle
      };
    }

    return {
      available: true,
      handle,
      address: instance.address,
      id: instance.id,
      objectIndex: instance.objectIndex
    };
  }

  resolveInstance(rvalueAddress) {
    return this.resolveInstanceNative(rvalueAddress);
  }

  resolveCInstance(handle) {
    const address = this.resolveCInstanceNative(handle);
    if (address.isNull()) {
      return null;
    }
    return {
      address: address.toString(),
      pointer: address,
      id: address.add(CINSTANCE_ID_OFFSET).readU32(),
      objectIndex: address.add(CINSTANCE_OBJECT_INDEX_OFFSET).readU32(),
      spriteIndex: address.add(CINSTANCE_SPRITE_INDEX_OFFSET).readS32()
    };
  }

  getPropertyRValue(handle, name, arrayIndex = NO_ARRAY_INDEX) {
    const result = Memory.alloc(RVALUE_SIZE);
    clearRValue(result);
    const success = this.getVarNative(
      handle,
      this.variableId(name),
      arrayIndex,
      result
    );
    return success === 0 ? null : result;
  }

  getProperty(handle, name, arrayIndex = NO_ARRAY_INDEX) {
    const result = this.getPropertyRValue(handle, name, arrayIndex);
    return result === null ? undefined : this.decodeRValue(result);
  }

  setProperty(handle, name, value, arrayIndex = NO_ARRAY_INDEX) {
    const current = this.getProperty(handle, name, arrayIndex);
    const preferredKind =
      name.endsWith("DamageMask")
        ? RValueKind.int64
        : current === undefined
          ? null
          : Object.entries(RValueKind)
              .find(([key]) => key === current.kind)?.[1] ?? null;
    const input = this.makeRValue(value, preferredKind);
    const success = this.setVarNative(
      handle,
      this.variableId(name),
      arrayIndex,
      input
    );
    if (success === 0) {
      throw new Error(`GameMaker rejected ${name}[${arrayIndex}]`);
    }
    return value;
  }

  getArray(handle, name, logicalLengthName = null) {
    const property = this.getPropertyRValue(handle, name);
    if (property === null) {
      return undefined;
    }
    const decoded = this.decodeRValue(property);
    if (decoded.kind !== "array") {
      throw new TypeError(`${name} is ${decoded.kind}, not an array`);
    }

    const array = property.readPointer();
    if (array.isNull()) {
      return [];
    }
    const physicalLength = array.readU32();
    if (physicalLength > MAX_ARRAY_LENGTH) {
      throw new RangeError(`${name} has suspicious length ${physicalLength}`);
    }

    let length = physicalLength;
    if (logicalLengthName !== null) {
      const logical = this.getProperty(handle, logicalLengthName);
      if (logical && Number.isFinite(logical.value)) {
        length = Math.min(Math.trunc(logical.value), physicalLength);
      }
    }

    const values = [];
    for (let index = 0; index < length; ++index) {
      values.push(this.getProperty(handle, name, index));
    }
    return values;
  }

  item(handle, index = null) {
    const name =
      this.getProperty(handle, "Name")?.value ??
      this.getProperty(handle, "BaseName")?.value ??
      this.getProperty(handle, "Description")?.value ??
      "(unnamed)";
    const runtime = this;
    const item = {
      index,
      handle,
      name,
      type: this.getProperty(handle, "Type")?.value,
      instance: this.resolveCInstance(handle),

      get(propertyName, arrayIndex = NO_ARRAY_INDEX) {
        return runtime.getProperty(handle, propertyName, arrayIndex)?.value;
      },

      set(propertyName, value, arrayIndex = NO_ARRAY_INDEX) {
        return runtime.setProperty(handle, propertyName, value, arrayIndex);
      },

      getArray(propertyName, logicalLengthName = null) {
        return runtime
          .getArray(handle, propertyName, logicalLengthName)
          ?.map(entry => entry?.value);
      },

      properties(propertyNames) {
        const result = {};
        for (const propertyName of propertyNames) {
          result[propertyName] = this.get(propertyName);
        }
        return result;
      },

      allProperties(includeKinds = false) {
        return runtime.dumpProperties(handle, includeKinds);
      },

      propertyNames() {
        return runtime.variableNames();
      },

      toJSON() {
        return {
          index: this.index,
          handle: this.handle,
          name: this.name,
          type: this.type,
          instance: this.instance
        };
      }
    };

    return new Proxy(item, {
      get(target, property, receiver) {
        if (typeof property !== "string" || Reflect.has(target, property)) {
          return Reflect.get(target, property, receiver);
        }
        if (!runtime.getVariableIds().has(property)) {
          return undefined;
        }

        const value = runtime.getProperty(handle, property);
        if (value?.kind === "array") {
          const logicalLength =
            property === "Traits" ? "TraitCount" : null;
          return target.getArray(property, logicalLength);
        }
        return value?.value;
      },

      set(target, property, value, receiver) {
        if (
          typeof property === "string" &&
          runtime.getVariableIds().has(property)
        ) {
          runtime.setProperty(handle, property, value);
          if (property === "Name") {
            target.name = value;
          }
          return true;
        }
        return Reflect.set(target, property, value, receiver);
      }
    });
  }

  inventory() {
    const character = this.currentCharacter();
    if (!character.available) {
      throw new Error(character.reason);
    }

    const countValue = this.getProperty(
      character.handle,
      "InventoryCount"
    );
    const count =
      countValue && Number.isFinite(countValue.value)
        ? Math.min(Math.trunc(countValue.value), 256)
        : 256;

    const items = [];
    for (let index = 0; index < count; ++index) {
      const slot = this.getProperty(
        character.handle,
        "Inventory",
        index
      );
      if (slot === undefined) {
        break;
      }
      if (!["real", "int32", "int64"].includes(slot.kind)) {
        continue;
      }
      const handle = Math.trunc(slot.value);
      if (handle > 0) {
        items.push(this.item(handle, index));
      }
    }
    return items;
  }

  dumpProperties(handle, includeKinds = false) {
    const result = {};
    for (const name of this.variableNames()) {
      const value = this.getProperty(handle, name);
      if (
        value === undefined ||
        value.kind === "undefined" ||
        value.kind.startsWith("unknown:")
      ) {
        continue;
      }
      result[name] = includeKinds ? value : value.value;
    }
    return result;
  }

  writeJson(path, value) {
    const file = new File(path, "w");
    try {
      file.write(JSON.stringify(value, null, 2));
    } finally {
      file.close();
    }
  }

  watchScript(rva, name = `script+0x${rva.toString(16)}`) {
    const runtime = this;
    const listener = Interceptor.attach(this.executableAddress(rva), {
      onEnter(args) {
        this.argc = args[3].toInt32();
        this.argv = args[4];
        console.log(
          `[${name}] enter tid=${this.threadId} depth=${this.depth} ` +
          `self=${args[0]} other=${args[1]} argc=${this.argc}`
        );
        const count = Math.max(0, Math.min(this.argc, 16));
        for (let index = 0; index < count; ++index) {
          const value = this.argv
            .add(index * Process.pointerSize)
            .readPointer();
          if (value.isNull()) {
            console.log(`  argv[${index}]=null`);
            continue;
          }
          try {
            console.log(
              `  argv[${index}]=${JSON.stringify(runtime.decodeRValue(value))}`
            );
          } catch (error) {
            console.log(`  argv[${index}]=<unreadable: ${error}>`);
          }
        }
      },
      onLeave(retval) {
        console.log(`[${name}] leave return=${retval}`);
      }
    });
    this.listeners.add(listener);
    return listener;
  }

  watchInventoryClicks() {
    const install = (rva, action) => {
      const runtime = this;
      const listener = Interceptor.attach(this.executableAddress(rva), {
        onEnter(args) {
          const argc = args[3].toInt32();
          const argv = args[4];
          if (argc < 1 || argv.isNull()) {
            return;
          }
          const itemRValue = argv.readPointer();
          if (itemRValue.isNull()) {
            return;
          }
          const reference = runtime.decodeRValue(itemRValue);
          if (!["real", "int32", "int64"].includes(reference.kind)) {
            return;
          }
          const handle = Math.trunc(reference.value);
          console.log(JSON.stringify({
            action,
            ...runtime.item(handle)
          }));
        }
      });
      this.listeners.add(listener);
    };

    install(Rva.assignAsPrimaryItem, "assign-primary");
    install(Rva.assignAsSecondaryItem, "assign-secondary");
  }

  watchProperty(handle, name, intervalMs = 250) {
    let previous = JSON.stringify(this.getProperty(handle, name));
    const timer = setInterval(() => {
      const current = this.getProperty(handle, name);
      const serialized = JSON.stringify(current);
      if (serialized !== previous) {
        console.log(`${name}: ${previous} -> ${serialized}`);
        previous = serialized;
      }
    }, intervalMs);
    this.timers.add(timer);
    return timer;
  }

  stopAll() {
    for (const listener of this.listeners) {
      listener.detach();
    }
    for (const timer of this.timers) {
      clearInterval(timer);
    }
    this.listeners.clear();
    this.timers.clear();
  }
}
