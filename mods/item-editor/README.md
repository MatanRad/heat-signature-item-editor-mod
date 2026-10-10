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
- non-grenade gadgets with capacity, recharge, self-charging, and applicable
  standard, long, and extreme range presets, plus capability-gated advanced
  controls.

The editor updates the gun's runtime fields and controlled trait entries,
rebuilds its display name, and asks the game to recalculate its sprite,
attachment, and animations.

Selecting a primary preset resets only the advanced values that it controls.
Infinite ammunition remains unchanged by preset selection.

## Building and installation

ItemEditor requires loader API version 3 and ImGui with Win32/DX9 bindings.
See the [repository build guide](../../README.md#building) for shared
prerequisites and configuration. Configure from the repository root, not this
directory.

`HS_BUILD_ITEM_EDITOR` defaults to `ON`. Build just this mod from the
repository root:

```powershell
cmake --build build --config Release --target ItemEditor
```

Copy `build\bin\Release\ItemEditor.dll` into the game's `mods` folder, or set
`HEATSIG_DIR` during configuration to copy it automatically after each build.

## Architecture

- [ItemEditor.cpp](src/ItemEditor.cpp) wires loader hooks to the editor.
- [ItemEditorWindow](src/ItemEditorWindow.h) owns shared ImGui session state and the
  render-to-game-thread handoff.
- [ItemEditorLayout.h](src/ItemEditorLayout.h) defines the item-layout seam used by the shared
  window controller.
- [GunEditorLayout](src/GunEditorLayout.h) adapts gun editing to that layout seam.
- [GunEditor](src/GunEditor.h) translates gun parameters into Heat Signature properties.
- [MeleeEditorLayout](src/MeleeEditorLayout.h) adapts melee editing to the layout seam.
- [MeleeEditor](src/MeleeEditor.h) translates melee parameters into Heat Signature
  properties and traits.
- [GadgetEditorLayout](src/GadgetEditorLayout.h) adapts gadget editing to the layout seam.
- [GadgetEditor](src/GadgetEditor.h) translates shared and family-specific gadget parameters
  into Heat Signature properties and traits.
- [GameMakerPropertyAccess](src/GameMakerPropertyAccess.h) provides mod-local typed property access.
- [Gun research](docs/gun-research.txt) records observed gun-property and sprite behavior.
- [Melee research](docs/melee-research.txt) records observed melee-property behavior.
- [Gadget research](docs/gadget-research.md), [range research](docs/gadget-range-ida-research.md),
  and [object identity research](docs/ida-object-identity-research.md) record gadget evidence.
- [Frida guide](../../frida/README.md) explains dynamic exploration and the
  reusable Frida agents.
