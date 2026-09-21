# Heat Signature Item Editor

A native item editor mod for
[Heat Signature Mod Loader](https://github.com/piepieonline/HeatSignatureModLoader).

## Features

Hold **Ctrl** and click a supported item in the inventory to open its editor.
Changes apply automatically on the game thread.

The gun editor currently supports:

- lethal or concussive damage;
- loud, quiet, or silenced sound behavior;
- normal, quickfire, or automatic fire rate;
- regular or armour-piercing ammunition.
- collapsed advanced controls for noise, wall audibility, firing interval,
  concussive uses and capacity, and infinite ammunition.
- melee weapons with damage, armour-piercing, dash range, auto-strike,
  recovery, strike noise, and knockback presets;
- collapsed melee advanced controls for custom dash range, strike interval,
  sound radius, knockback speed, and dash speed.

The editor updates the gun's runtime fields and controlled trait entries,
rebuilds its display name, and asks the game to recalculate its sprite,
attachment, and animations.

Selecting a primary preset resets only the advanced values that it controls.
Infinite ammunition remains unchanged by preset selection.

## Requirements

- A 32-bit Windows build of Heat Signature Mod Loader with API version 3.
- A local checkout of the mod loader for its public headers.
- CMake 3.26 or newer.
- Visual Studio 2022 with the Win32 C++ toolchain.
- vcpkg with the `imgui` package and Win32/DX9 bindings.

## Building

The default layout expects this repository and the loader to be siblings:

```text
projects/
  HeatSignatureModLoader/
  HeatSignatureItemEditor/
```

Configure and build:

```powershell
cmake -S . -B build -A Win32 `
  -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x86-windows-static

cmake --build build --config Release
```

Override `HEATSIG_MODLOADER_DIR` if the loader checkout is elsewhere. Set
`HEATSIG_DIR` during configuration to copy `ItemEditor.dll` into the game's
`mods` folder after each build.

## Architecture

- `src/ItemEditor.cpp` wires loader hooks to the editor.
- `src/ItemEditorWindow.*` owns shared ImGui session state and the
  render-to-game-thread handoff.
- `src/ItemEditorLayout.h` defines the item-layout seam used by the shared
  window controller.
- `src/GunEditorLayout.*` adapts gun editing to that layout seam.
- `src/GunEditor.*` translates gun parameters into Heat Signature properties.
- `src/MeleeEditorLayout.*` adapts melee editing to the layout seam.
- `src/MeleeEditor.*` translates melee parameters into Heat Signature
  properties and traits.
- `src/GameMakerPropertyAccess.*` provides mod-local typed property access.
- `docs/gun-research.txt` records observed gun-property and sprite behavior.
- `docs/melee-research.txt` records observed melee-property behavior.
- [`frida/README.md`](frida/README.md) explains dynamic exploration and the
  reusable Frida agents.
