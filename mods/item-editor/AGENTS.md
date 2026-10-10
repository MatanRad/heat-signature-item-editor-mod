# ItemEditor agent guide

## Scope

ItemEditor is a standalone native mod in this multi-mod repository. Keep
item-specific behavior here and follow the repository-wide
[agent guide](../../AGENTS.md).

## Layout

- [ItemEditor.cpp](src/ItemEditor.cpp): composition root and GameMaker hook adapters.
- [ItemEditorWindow](src/ItemEditorWindow.h): ImGui session state and render/game-thread handoff.
- [GunEditor](src/GunEditor.h): gun-domain snapshot and mutation rules.
- [MeleeEditor](src/MeleeEditor.h) and [GadgetEditor](src/GadgetEditor.h): item-family mutation rules.
- [GameMakerPropertyAccess](src/GameMakerPropertyAccess.h): private typed wrapper over `HS_ModApi`.
- [Gun research](docs/gun-research.txt): observed runtime behavior; consult when changing gun semantics.
- [Frida guide](../../frida/README.md): use for Frida workflows, native ABI facts, and safety boundaries.
- [Item editor prototype](../../frida/archive/item-editor-prototype.js): historical reference, not production code.

## Design rules

- Keep GameMaker reads, writes, and script calls on a game-thread hook.
- Keep ImGui callbacks limited to mod-owned state and one pending latest-state update.
- Preserve unrelated item traits when normalizing editor-controlled traits.
- Keep reverse-engineered offsets and generic loader facilities out of this repository
  when the public loader API already provides the operation.
- Treat behavior recorded in [docs/](docs/) as evidence, not a stable upstream contract.

## Validation

Configure from the repository root against the sibling loader checkout and
root vcpkg manifest, then build the Win32 `ItemEditor` target. A change is
complete when the target compiles and temporary build output is removed.

## License

New source is CC BY-NC 4.0. Preserve the copyright notice, attribution,
license reference, and change notice when redistributing or adapting it.
