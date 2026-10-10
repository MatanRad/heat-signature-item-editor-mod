# Heat Signature Mods

Independent native mods for
[Heat Signature Mod Loader](https://github.com/piepieonline/HeatSignatureModLoader).
Each mod builds into its own DLL and can be installed separately.

## Mods

| Mod | Description | CMake target | Build option |
|---|---|---|---|
| [Item Editor](mods/item-editor/README.md) | Ctrl-click inventory items to edit guns, melee weapons, and gadgets | `ItemEditor` | `HS_BUILD_ITEM_EDITOR` (default `ON`) |

## Requirements

- A 32-bit Windows build of Heat Signature Mod Loader. See each mod's README
  for its required loader API version.
- A local checkout of the loader for its public headers.
- CMake 3.26 or newer.
- Visual Studio 2022 with the Win32 C++ toolchain.
- vcpkg; the root [manifest](vcpkg.json) pins the shared dependency baseline.
  It currently includes ImGui with Win32/DX9 bindings for ItemEditor.

## Building

Configure from the repository root. By default, the loader checkout is expected
at `..\HeatSignatureModLoader`; the name of this repository's directory does
not matter. Override `HEATSIG_MODLOADER_DIR` if the loader is elsewhere.

```powershell
cmake -S . -B build -A Win32 `
  -DCMAKE_TOOLCHAIN_FILE=C:\path\to\vcpkg\scripts\buildsystems\vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x86-windows-static

cmake --build build --config Release
```

The build command builds all enabled mods. To build only ItemEditor:

```powershell
cmake --build build --config Release --target ItemEditor
```

Use each mod's `HS_BUILD_*` option at configuration time to exclude it from the
build, for example `-DHS_BUILD_ITEM_EDITOR=OFF`. These options select DLL
targets, not vcpkg manifest dependencies.

If reusing a build directory configured before this restructuring, run the
configure command again. The generated Visual Studio solution is now named
`HeatSignatureMods.sln`; the ItemEditor target and DLL name are unchanged.

## Installation

DLLs are written to `build\bin\<configuration>\`, for example
`build\bin\Release\ItemEditor.dll`. Copy only the mods you want into the game's
`mods` folder.

Alternatively, set `HEATSIG_DIR` during configuration to copy each DLL there
after it builds:

```powershell
cmake -S . -B build `
  "-DHEATSIG_DIR=C:\Program Files (x86)\Steam\steamapps\common\Heat Signature"
```

Disabling a build option does not remove an already installed DLL.

## Repository layout

- [mods/](mods/) contains each mod's implementation, build target, user guide,
  and supporting research.
- [cmake/HeatSignatureMods.cmake](cmake/HeatSignatureMods.cmake) applies common
  target defaults and optional deployment.
- [frida/](frida/README.md) contains shared investigation tools and historical
  prototypes, not production mod code.
- [AGENTS.md](AGENTS.md) describes repository-wide development rules.

Loader internals remain in the separate loader repository. Runtime state and
item-specific behavior remain owned by their mod.

## Adding a mod

1. Create `mods\<mod-name>\` with a `CMakeLists.txt`, `README.md`, and `src\`.
2. Define a uniquely named `SHARED` target with its own loader entry point.
3. Call `heat_signature_configure_mod(<target>)` to apply C++17, loader header
   access, DLL output location, and optional deployment. Set the mod's own
   include directories and link only its required dependencies.
4. Add an explicit `HS_BUILD_*` option and guarded `add_subdirectory` to the
   root [CMakeLists.txt](CMakeLists.txt), and add the mod to the catalog above.
5. Keep mod-specific research and tests with the mod. Extract shared C++ code
   only when multiple implemented mods need it; do not share mutable state.
6. Build and test the mod independently, then alongside other installed mods,
   checking hook interactions and shared ImGui visibility where applicable.

## License

Source is licensed under [CC BY-NC 4.0](LICENSE.md). Preserve required
attribution, license references, and change notices when redistributing or
adapting it, including when packaging individual mods.
