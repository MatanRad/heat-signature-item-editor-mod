# Heat Signature Mods agent guide

## Scope

This repository contains independently installable native mods for Heat
Signature Mod Loader, with one DLL per mod. Keep loader changes and generic
loader facilities in the sibling `HeatSignatureModLoader` repository.

## Layout

- [CMakeLists.txt](CMakeLists.txt): repository configuration and explicit mod selection.
- [cmake/HeatSignatureMods.cmake](cmake/HeatSignatureMods.cmake): shared target defaults and optional deployment.
- [vcpkg.json](vcpkg.json): shared dependency manifest and baseline.
- [mods/](mods/): mod-owned sources, build targets, documentation, research, and tests.
- [ItemEditor guide](mods/item-editor/AGENTS.md): item-specific development rules.
- [Frida guide](frida/README.md): shared investigation workflows, ABI facts, and safety limits.

## Design rules

- Keep each mod's entry point, hooks, UI, and mutable runtime state independent.
- Keep GameMaker reads, writes, and script calls on game-thread hooks.
- Keep ImGui callbacks limited to mod-owned state and queued updates.
- Use public loader operations instead of embedding reverse-engineered offsets
  or duplicating loader facilities in production mods.
- Keep mod-specific behavior and research with its owning mod. Treat research
  as observed evidence, not a stable upstream contract.
- Extract shared C++ modules only for concrete reuse by multiple implemented
  mods. Prefer static linking without shared mutable state or a support DLL.
- Link dependencies per mod; the shared build helper does not impose ImGui.
- Register mods explicitly in the root build and catalog. Do not add empty
  placeholder mods or a runtime framework.

## Validation

Configure from the repository root for Win32 using the sibling loader checkout,
root vcpkg manifest, and `x86-windows-static` triplet. Build the affected mod
target; build all enabled mods when shared build configuration changes.
Use fresh temporary output where appropriate and remove only output created
for the task. Do not remove existing build output.

For new mods or runtime integration changes, test independently and alongside
other installed mods, especially hook interactions and shared ImGui visibility.

## License

New source is CC BY-NC 4.0. Preserve the copyright notice, attribution,
[license reference](LICENSE.md), and change notice when redistributing or
adapting it.
