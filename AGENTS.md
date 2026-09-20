# ItemEditor agent guide

## Scope

ItemEditor is a standalone native mod for Heat Signature Mod Loader. Keep
loader changes in the sibling `HeatSignatureModLoader` repository; keep
item-specific behavior here.

## Layout

- `src/ItemEditor.cpp`: composition root and GameMaker hook adapters.
- `src/ItemEditorWindow.*`: ImGui session state and render/game-thread handoff.
- `src/GunEditor.*`: gun-domain snapshot and mutation rules.
- `src/GameMakerPropertyAccess.*`: private typed wrapper over `HS_ModApi`.
- `docs/gun-research.txt`: observed runtime behavior; consult when changing gun semantics.
- `frida/README.md`: use for Frida workflows, native ABI facts, and safety boundaries.
- `frida/archive/item-editor-prototype.js`: historical reference, not production code.

## Design rules

- Keep GameMaker reads, writes, and script calls on a game-thread hook.
- Keep ImGui callbacks limited to mod-owned state and one pending latest-state update.
- Preserve unrelated item traits when normalizing editor-controlled traits.
- Keep reverse-engineered offsets and generic loader facilities out of this repository
  when the public loader API already provides the operation.
- Treat behavior recorded in `docs/` as evidence, not a stable upstream contract.

## Validation

Configure a Win32 build against the sibling loader checkout and the repository
vcpkg manifest, then build `ItemEditor`. A change is complete when the target
compiles and temporary build output is removed.

## License

New source is CC BY-NC 4.0. Preserve the copyright notice, attribution,
license reference, and change notice when redistributing or adapting it.
