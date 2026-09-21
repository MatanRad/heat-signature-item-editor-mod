# Frida guide for Heat Signature

This directory contains reusable Frida agents for exploring the 32-bit Windows
build of Heat Signature. The tools are intended for short, controlled
experiments before behavior is moved into a native mod.

Frida is most useful here for observation:

- validating recovered RVAs;
- inspecting script arguments and return values;
- discovering the current player and inventory;
- reading the runtime variable registry;
- comparing item properties;
- watching low-frequency scripts and property changes.

Native calls and writes are possible, but they carry more risk because Frida's
JavaScript thread is not automatically the GameMaker game thread.

## Install

Install the Frida command-line tools:

```powershell
py -m pip install frida-tools
frida --version
```

Install the JavaScript bundler used by this directory:

```powershell
cd frida
npm install
npm run build
```

Compiled agents are written to `frida/dist/`.

References:

- [Frida installation](https://frida.re/docs/installation/)
- [Frida modes of operation](https://frida.re/docs/modes/)
- [Frida JavaScript API](https://frida.re/docs/javascript-api/)

## Attach

For normal investigation, launch the game first and attach after it reaches a
stable screen:

```powershell
frida -n Heat_Signature.exe -l .\dist\toolkit.js
```

Attaching late avoids interfering with Steam startup, the D3D9 proxy, and
ModLoader initialization.

Spawn mode is useful only when investigating early process startup:

```powershell
frida -f "C:\path\to\Heat_Signature.exe" -l .\dist\process-info.js
```

The toolkit rejects any target that is not Windows IA-32 with four-byte
pointers.

## Agents

| Agent | Purpose |
|---|---|
| `toolkit.js` | Interactive `hs` object plus common REPL helpers |
| `process-info.js` | Print architecture, module bases, sizes, and paths |
| `current-character.js` | Read and validate `global.Player` |
| `inventory.js` | Print the current live character's inventory |
| `dump-item-properties.js` | Dump every defined property on an inventory item |
| `dump-variable-names.js` | Print the GameMaker runtime variable registry |
| `watch-inventory-clicks.js` | Observe primary and secondary item assignment |
| `watch-property.js` | Poll one property and report changes |
| `watch-script.js` | Attach an argument logger to a script RVA |

The source agents live under `frida/src/`. Edit those files and run
`npm run build`; do not edit generated files in `dist/`.

## Interactive toolkit

Load `dist/toolkit.js`, then use these helpers at the Frida prompt:

```js
hs.info()
currentCharacter()
inventory()
itemProperties(0)
itemProperties(0, true) // include RValue kinds
```

Read one property:

```js
gun = inventory().find(item => item.type === "Gun")
gun.Value
gun.Ammo
hs.getProperty(gun.handle, "WeaponDamageMask")
```

Read an array using a separate logical count:

```js
gun.Traits
hs.getArray(gun.handle, "Traits", "TraitCount")
```

Write a property:

```js
gun.Ammo = 72
gun.set("WeaponDamageMask", 5)
hs.setProperty(gun.handle, "Ammo", 72)
hs.setProperty(gun.handle, "WeaponDamageMask", 5)
```

Dump data to a file:

```js
hs.writeJson(
  "C:\\temp\\gun.json",
  hs.dumpProperties(gun.handle, true)
)
```

Stop every listener and timer installed through the toolkit:

```js
hs.stopAll()
```

## Script tracing

Every entry in the loader's `docs/ScriptFunctions.txt` is an RVA relative to
`Heat_Signature.exe`:

```text
runtime address = module base + RVA
```

ASLR changes the module base between launches. Never treat an RVA as an
absolute runtime address.

Load the trace agent:

```powershell
frida -n Heat_Signature.exe -l .\dist\watch-script.js
```

Then attach by RVA:

```js
watchScript(0x000a0080, "gml_Script_PlayAsCharacter")
```

The logger caps argument traversal at 16 entries and prints each readable
`RValue`. Use low-frequency scripts first. Logging a per-frame or inner-loop
script can make the game unusable or change timing-sensitive behavior.

## Known native ABI

Compiled `gml_Script_*` functions use:

```cpp
RValue* __cdecl Script(
    CInstance* self,
    CInstance* other,
    RValue* result,
    int argc,
    RValue** argv);
```

In Frida, Microsoft x86 cdecl is named `mscdecl`, not `cdecl`:

```js
new NativeFunction(address, "pointer", [
  "pointer",
  "pointer",
  "pointer",
  "int",
  "pointer"
], "mscdecl")
```

Using the wrong x86 calling convention can corrupt the stack.

## Known layouts

### `CInstance`

```text
+0x78  uint32 instance ID
+0x7c  uint32 object index
+0x8c  int32 sprite_index
```

The `sprite_index` offset was established by comparing live gun instances with
the sprite IDs serialized in their checksum strings.

### `RValue`

```text
+0x00  8-byte value payload
+0x08  unknown/flags
+0x0c  kind plus upper-bit flags
size   0x10
```

The runner masks the kind with `0x00ffffff`. Confirmed useful kinds:

| Kind | Payload |
|---:|---|
| 0 | `double` |
| 1 | `YYString*` |
| 2 | array pointer |
| 3 | pointer |
| 5 | undefined |
| 7 | signed 32-bit integer |
| 10 | signed 64-bit integer |
| 13 | double-backed numeric; semantic name not established here |

Kind 10 is confirmed by the runner's numeric conversion routine at RVA
`0x00CBC130`, which uses `fild qword ptr [value]`.

### `YYString`

```text
+0x00  char* text
+0x04  uint32 refcount
+0x08  uint32 length
```

Create strings through the engine's `SetString` function. Replacing a
`YYString*` or editing its text buffer directly can break ownership and
reference counting.

## Known runtime locations

These values target the game build used by this project:

| Purpose | RVA |
|---|---:|
| Global variable container | `0x0453D610` |
| Variable registry | `0x0453D5B8` |
| Resolve instance reference | `0x00CBC420` |
| Resolve live `CInstance*` | `0x00C98DF0` |
| Read variable | `0x00C99410` |
| Write variable | `0x00C996F0` |
| Create GameMaker string | `0x00CAB130` |
| Assign primary item | `0x004CF140` |
| Assign secondary item | `0x004D02A0` |

These offsets are version-specific. Revalidate them after a game update.

## Current character and inventory

`gml_Script_PlayAsCharacter` writes its first argument to global variable ID
5, `global.Player`.

At runtime:

```text
*(game base + 0x0453D610) -> global container
global variable ID 5     -> Player RValue
```

On the character-selection screen, `global.Player` may contain a small value
such as `49`. That is not a live character. Always validate it through
`ResolveCInstance`; live instance handles are generally `100000` or greater.

The current character's `Inventory[index]` values are item handles.
`InventoryCount` provides the expected logical count.

## Property and array behavior

The runtime variable registry contains every known variable name in the game,
not only properties defined on one object. A gun does not have thousands of
meaningful properties.

`dumpProperties()` probes the registry and filters failed, undefined, and
unknown values. This performs thousands of native calls, so use it as a
one-time investigation rather than a polling operation.

GameMaker arrays may have physical spare capacity. For `Traits`, use
`TraitCount` as the logical length:

```js
hs.getArray(gun.handle, "Traits", "TraitCount")
```

Editing a JavaScript array returned by the toolkit does not change the game.
Use `setProperty(handle, name, value, index)` for an indexed write.

## Threading and safety

Frida JavaScript callbacks and RPC calls are not guaranteed to execute on the
GameMaker game thread.

Relatively safe operations:

- reading mapped memory;
- inspecting module ranges and instructions;
- observing an existing game-thread call with `Interceptor`;
- copying bounded scalar or string data for later display.

Higher-risk operations:

- calling a `gml_Script_*` function from the REPL;
- writing several correlated properties independently;
- invoking engine registries from an arbitrary thread;
- retaining `CInstance*` or engine string pointers after their lifetime;
- tracing hot functions with verbose logging.

`Process.runOnThread()` is not a general solution. Interrupting a thread while
it owns locks or is inside non-reentrant engine code can deadlock or corrupt
state.

For production mods, queue complete commands from UI or worker threads and
consume them in a known game-thread hook.

## Hook interaction

ModLoader uses MinHook on `gml_Script_*` entries. Frida `Interceptor` may patch
the same address. If results are inconsistent:

1. Test without mods subscribing to the target script.
2. Confirm the address still begins at a valid function entry.
3. Attach after the loader has finished installing hooks.
4. Compare the observed arguments in a native debugger.

Prefer observation hooks over replacement hooks. A replacement must call the
correct original trampoline, preserve the ABI, and handle reentrancy.

## Practical workflow

1. Use `process-info.js` to confirm IA-32 and module bases.
2. Validate a recovered RVA with `hs.executableAddress(rva)`.
3. Observe a low-frequency script with `watchScript`.
4. Confirm argument kinds and the calling convention in x32dbg or IDA.
5. Read related instance properties.
6. Change one value at a time on a disposable save.
7. Record the behavior and restore the original value.
8. Move stable behavior into a native mod with game-thread execution.

The archived `frida/archive/item-editor-prototype.js` records the exploratory
path used to build ItemEditor. It contains superseded type assumptions and
should not be used as the current toolkit.
