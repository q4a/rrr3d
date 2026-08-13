# Milestone 10: autonomous Apple Silicon application bundle

## Result

Milestone 10 is complete on top of the corrected M5–M9.5 original-data path.
The Debug and Release presets package the same `MainMenu2`, MusicCat,
original race/session/HUD/effects runtime, bgfx/Metal renderer and
source-calibrated Jolt adapter that were accepted in the preceding milestones.
They also package the native AVFoundation replacement for the legacy
DirectShow player. The cancelled standalone portable race is not compiled by
either bundle preset.

The canonical artifacts are:

```text
build/macos-arm64-m10/Debug/RRR3d.app
build/macos-arm64-m10/RRR3d-1.3.1-macos-arm64-Debug.zip
build/macos-arm64-release/Release/RRR3d.app
build/macos-arm64-release/Release/RRR3d-1.3.1-macos-arm64.zip
```

## Bundle contents

Both applications are native `arm64` bundles with minimum macOS 13.0,
identifier `org.rrr3d.motorrock` and version 1.3.1. SDL3, Ogg/Vorbis,
bgfx/bx/bimg and Jolt are linked statically, so `Contents/Frameworks` is
intentionally empty. Runtime dependencies resolve only to Apple system
libraries and frameworks.

`Contents/Resources/game-data` is copied from
`legacy-assets.catalog`, not by recursively copying the working directory.
It contains exactly 1196 imported Motor Rock assets and four port metadata/UI
files. This excludes untracked File Provider duplicates. The application
resolves this directory relative to its own executable and does not depend on
the build tree or the Terminal working directory.

The bundle also contains `Contents/Resources/video-cache` with all 14 original
movies remuxed from AVI to MP4 without re-encoding their H.264/MP3 streams, the
native icon and all project/dependency notices. FFmpeg is only a build-time
tool. The application is ad-hoc signed after resources are copied.

## Verification

`scripts/verify_macos_bundle.sh` now rejects a bundle unless all of the
following are true:

- package type `APPL`, identifier `org.rrr3d.motorrock` and version 1.3.1;
- arm64-only Mach-O and `LC_BUILD_VERSION` minimum 13.0;
- valid strict code signature;
- empty `Contents/Frameworks`;
- no Homebrew, local build, Windows or other external absolute dependency;
- valid icon/notices/catalog/manifest and exactly 1200 game-data files;
- exactly 14 cached MP4 movies and no unexpected files in `video-cache`.

The Release post-build invokes this verifier through its xattr-free transport
copy. This is deliberate for File Provider-backed workspaces, which can
reattach `com.apple.FinderInfo` to the build-directory `.app` immediately
after `xattr -cr`; the signed bundle contents are still verified strictly.

Both Debug and Release bundles pass this verifier. Both also pass:

- original resource/MainMenu2 and selected-race provenance audit;
- map1/Jolt physics and complete race-state smoke;
- SDL keyboard/mouse/virtual-gamepad input;
- 182 original Ogg audit and the MusicCat background decode, shuffle,
  automatic/manual Next, pause/resume and state round-trip;
- AVFoundation playback, displayed frame, near-end seek, completion and
  `cVideoStopped` tournament callback of an original H.264/MP3 movie;
- bgfx/Metal original menu dispatch and 240-frame M9.5 race render smoke.

The final race smoke starts through
`Single Player -> Tournament -> Continue -> Start race`. It verifies six
cars, four player wheel contacts, both original camera modes, HUD, pause
dialog/frozen world, shield, resize, cube reflection, both projected shadow
splits, Scene/HDR/adaptation/bloom/composite and FxTrail. No enlarged wheel
meshes or rotating garbage were observed.

## Transport archives and moved launch

The packaging script creates an xattr-free transport copy, verifies its
signature and tests the ZIP before publishing it.

```text
Debug ZIP
  size:   424458770 bytes
  SHA256: 4104575ddad2895faeac931b7ea4f4229da9a349fee52c1352f3827856909d68

Release ZIP
  size:   421432643 bytes
  SHA256: b54bc775c89e4cffed48cc51ced087360eea9fabcdfef1db10f80783b8f39556
```

The Release ZIP was extracted to an independent `/private/tmp` directory.
The extracted application retained its signature and arm64 identity, resolved
all 1196 original assets from its moved `Contents/Resources`, and passed the
240-frame race smoke. A separate `open -W -n` invocation through macOS
LaunchServices also exited successfully.

## Remaining release work

The milestone criterion—an autonomous double-clickable `RRR3d.app`—is met.
The standard artifact is still ad-hoc signed. Public distribution additionally
requires confirmation that the original game data may be redistributed,
Developer ID signing, hardened runtime, notarization/stapling and a launch
check on a separate clean Mac. Physical Bluetooth/USB controllers and audio
device switching should also be checked on release hardware; Windows
regression remains a Windows CI task.
