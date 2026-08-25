# Milestone 6: original MainMenu2 resources on bgfx/Metal

## Acceptance path

The `macos-arm64-m6` preset no longer builds the earlier `PortableMenu`
vertical slice. It selects `RRR3D_BUILD_ORIGINAL_MENU` and the dedicated
`main_bgfx_original_menu.cpp` entry point.

The renderer-neutral `MainMenu2Spec.h` is the common source of truth for:

- the original Windows `MainMenu2.cpp` implementation;
- the macOS `OriginalMainMenu` resource adapter;
- the bgfx/Metal presentation path.

It contains the original image paths, string identifiers, font names and
sizes, version label, menu layout constants and selection colours. A visual
change to these values therefore affects both renderer paths instead of being
duplicated in a separately invented menu.

## Original data path

```text
resources/game-data
  -> ResourceFileSystem (legacy backslash normalization + exact-case checks)
  -> game.xml serialized Language record
  -> its Data/{language}.txt (legacy UTF-16LE StringLibrary token semantics)
  -> MainMenu2Spec string keys
  -> CoreText/Verdana text textures
  -> MainMenu2Spec GUI paths
  -> original DDS container / decoded original PNG pixels
  -> renderer::GraphicsDevice
  -> bgfx/Metal
```

`menu/menu.cfg` and `ui/font5x7.txt` are not read by this target. The displayed
labels come from all six shipped localization files. Duplicate string IDs use
the original `StringLibrary::Set` rule: the last value wins. The token parser
also deliberately preserves the source behavior for the unterminated French
`scMaslo` value instead of rejecting the full file.

The static main state uses:

- `Data/GUI/mainFrame.dds`;
- `Data/GUI/topPanel5.png`;
- `Data/GUI/bottomPanel5.png` (loaded, hidden in the initial state as in the
  original constructor);
- `Data/GUI/mainItemSel5.png`;
- `Data/GUI/cursor.png` (loaded for the following input milestone);
- the five original localized keys `svSingleGame`, `svNetGame`, `svOptions`,
  `svAuthors`, and `svExit`;
- Verdana at the heights declared by `ResourceManager::LoadGUI`.

The DDS stays encoded through the renderer boundary, preserving its DXT1
payload. PNG resources are decoded to RGBA8 by bimg and uploaded without
replacement artwork.

## Resource audit

Before opening a window the target validates:

- every entry in `legacy-assets.catalog` against its copied file size and
  exact-case path;
- every original `Data/GUI` PNG/DDS with bimg;
- every original `Data/GUI` R3D with the shared strict R3D decoder;
- the selected UTF-16LE localization file and required MainMenu2 keys;
- the selected language's serialized `file`, `locale`, `charset` and `primId`
  in `game.xml`, plus the complete six-language/two-commentator order.

The current copied data set reports 1,196 files / 553,107,397 bytes, 248 GUI
images and four GUI meshes. The exact localized-string count varies with the
shipped language file and its original token-stream parsing behavior.

## Build and verification

```bash
cmake --preset macos-arm64-m6
cmake --build --preset macos-arm64-m6 -j 8
build/macos-arm64-m6/Debug/RRR3d --language=russian --verify-resources
build/macos-arm64-m6/Debug/RRR3d --language=english --verify-resources
build/macos-arm64-m6/Debug/RRR3d --language=portuguese --verify-resources
build/macos-arm64-m6/Debug/RRR3d --language=french --verify-resources
build/macos-arm64-m6/Debug/RRR3d --language=spain --verify-resources
build/macos-arm64-m6/Debug/RRR3d --language=german --verify-resources
build/macos-arm64-m6/Debug/RRR3d --language=russian --smoke-test-frames=120
```

## Boundary

Milestone 6 proves the original main-menu resource model and initial visual
state on Metal. It does not claim that the complete legacy `gui::Manager` and
`GraphManager` object graph is already running on macOS: those classes still
contain D3D9-era graph resources outside the Milestone 5 renderer boundary.

The split made here is a migration seam, not a replacement game. The original
`MainMenu2.cpp` consumes the same specification and remains the owner of menu
state transitions. Milestone 7 must move event dispatch, selection changes and
widget actions across this seam; it must not restore `PortableMenu` or
hard-coded menu actions.
