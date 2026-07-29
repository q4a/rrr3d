# Building RRR3D / Motor Rock for macOS

## Supported system

- Apple Silicon (`arm64`): M1, M2, M3, M4 and newer;
- macOS 13.0 or newer;
- Xcode Command Line Tools with AppleClang;
- CMake 3.21 or newer, Ninja and Git;
- enough free space for the 530 MiB game-data package and build trees.

Rosetta, Wine, CrossOver and the Windows PhysX/DirectX libraries are not used.
The first configure may download pinned SDL3, TinyXML, libogg, libvorbis and
Jolt source archives. bgfx/bx/bimg artifacts are built once with:

```bash
scripts/build_bgfx_macos.sh
```

The original game resources must already be present in
`resources/game-data`. They can be imported without modifying the installed
game directory:

```bash
scripts/import_game_resources.sh '/path/to/Motor Rock'
```

Before distributing a build, verify that the game-data licence permits the
intended form of distribution.

## Current Milestone 10 application bundles

```bash
cmake --preset macos-arm64-m10
cmake --build --preset macos-arm64-m10 --parallel 8

cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release --parallel 8
```

Both presets run the corrected original-data path accepted in M5–M9.5:
`MainMenu2`, MusicCat, the original race/session/HUD/effects data,
source-calibrated Jolt physics and bgfx/Metal. They do not compile the
cancelled standalone portable race.

## Debug application bundle

```bash
cmake --preset macos-arm64-m10
cmake --build --preset macos-arm64-m10 -j 8
```

Result:

```text
build/macos-arm64-m10/Debug/RRR3d.app
```

## Release application bundle

```bash
cmake --preset macos-arm64-release
cmake --build --preset macos-arm64-release -j 8
```

Result:

```text
build/macos-arm64-release/Release/RRR3d.app
```

Both presets create a native application with this structure:

```text
RRR3d.app/Contents
├── Frameworks
├── Info.plist
├── MacOS/RRR3d
└── Resources
    ├── RRR3d.icns
    ├── Licenses
    └── game-data
```

SDL3, libogg, libvorbis, bgfx, bx and bimg are linked statically. Therefore
`Frameworks` is currently empty and the runtime link graph contains only
Apple system libraries and frameworks. The executable resolves resources from
`Contents/Resources` and does not depend on the Terminal working directory.

## Running

Open `RRR3d.app` in Finder, or run:

```bash
open build/macos-arm64-release/Release/RRR3d.app
```

The application writes only to:

```text
~/Library/Application Support/RRR3d
~/Library/Logs/RRR3d
```

Developer and smoke-test options can be passed to the inner executable:

```bash
build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --verify-resources

SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --physics-smoke-test

SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --audio-smoke-test

SDL_AUDIO_DRIVER=dummy \
  build/macos-arm64-release/Release/RRR3d.app/Contents/MacOS/RRR3d \
  --race-render-smoke-test --smoke-test-frames=240
```

## Bundle verification and transport ZIP

```bash
scripts/verify_macos_bundle.sh \
  build/macos-arm64-release/Release/RRR3d.app

scripts/package_macos_bundle.sh \
  build/macos-arm64-release/Release/RRR3d.app \
  build/macos-arm64-release/Release/RRR3d-1.3.1-macos-arm64.zip
```

The verifier checks package type, version 1.3.1, bundle identifier,
high-resolution support, arm64-only architecture, deployment target 13.0,
the intentionally empty `Frameworks`, exact resource completeness, strict code
signature and the absence of Homebrew, local build and Windows runtime
dependencies. It uses an xattr-free temporary transport copy because
file-provider-backed folders may attach Finder metadata to their contents.

The packaging script emits a clean ZIP whose contained `.app` retains its
signature. Move or extract that archive to test the application independently
of the source/build directory.

## Signing and notarization preparation

The standard presets use free local ad-hoc signing and do not require an Apple
Developer account. A Developer ID build can be configured with:

```bash
cmake --preset macos-arm64-release \
  -DRRR3D_MACOS_CODESIGN_IDENTITY='Developer ID Application: Example (TEAMID)' \
  -DRRR3D_MACOS_HARDENED_RUNTIME=ON
cmake --build --preset macos-arm64-release -j 8
```

With a real identity CMake requests a secure timestamp; hardened runtime adds
`codesign --options runtime`. Notarization still requires Apple credentials.
After signing, use `xcrun notarytool submit --wait`, staple the accepted ticket
with `xcrun stapler staple`, verify it, and only then create the final ZIP.

## Cleaning and diagnostics

Remove an individual generated build directory when a fully clean configure is
needed, then run its preset again. Useful diagnostics:

```bash
file RRR3d.app/Contents/MacOS/RRR3d
vtool -show-build RRR3d.app/Contents/MacOS/RRR3d
otool -L RRR3d.app/Contents/MacOS/RRR3d
codesign -d --verbose=4 RRR3d.app
```

Known runtime and distribution limitations are tracked in
`docs/MACOS_PORT_STATUS.md`. The completed bundle acceptance record, archive
hashes and moved-application launch result are in `docs/MILESTONE_10.md`.
