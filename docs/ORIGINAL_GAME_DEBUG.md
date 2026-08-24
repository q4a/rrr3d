# Original game debug mode on macOS

The macOS port exposes the useful parts of the original Windows debug code as
an explicit runtime mode. It is disabled during an ordinary launch and does
not enable the global `_DEBUG` branches from the Windows project.

## Launch

```sh
build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d --game-debug
```

Start or load a race normally. The diagnostic overlay appears in the upper
left corner. Its text textures are refreshed four times per second, while FPS
is measured every frame; this prevents the overlay itself from becoming a
performance problem.

## Controls

- `F1`: reset every race vehicle to its original serialized start position.
- `F2`: toggle the original Bloom/HDR post effects for the running process.
  This does not modify the profile.
- `F3`: toggle fullscreen without modifying the profile.
- `F4`, `F5`: intentionally do nothing. The corresponding Steam calls are
  commented out in the original `GameMode.cpp` as well.
- `F6`: show or hide the source AI trace. It is rendered without depth test,
  like the original `TraceGfx` debug actor.
- `F7`: let the ported source AI controller drive the player's existing car.
  No racer or physics actor is added.
- `F10`: show or hide the diagnostic overlay.
- `Page Up`, `Page Down`: switch between race/engine, live wheel/contact and
  serialized vehicle/suspension pages.

The original profile mappings `gaDebug1` through `gaDebug7` remain the source
of the F1-F7 bindings. F10 and the page keys are portable overlay controls.

## Deliberately excluded global debug behavior

The Windows `_DEBUG` define also changes normal game behavior: it can force
99 laps and cloudy weather, skip movies and startup timing, create a debug
AI path for a human car and enable other test-only branches. Those changes are
not part of `--game-debug`; the runtime mode preserves the selected campaign
race, roster, weather, lap count and presentation flow.

The shipped Windows executable is a Release build. Its F1-F3 non-retail
handlers may exist, but `AIDebug`, the physics HUD, F6/F7 control, PhysX VRD
and the other `_DEBUG` blocks were removed by the preprocessor. The complete
mode therefore cannot be enabled in that binary by a configuration switch;
the Windows sources must be rebuilt with the appropriate debug definitions.

## Verification

The Debug bundle is arm64 and is produced at:

```text
build/macos-arm64-m10/Debug/RRR3d.app
```

`--physics-smoke-test` covers the isolated debug state machine in addition to
the original race and vehicle regressions. `--input-smoke-test` validates the
original `gaDebug1/F1` mapping and the portable F10 mapping. The integrated
renderer path can be exercised with:

```sh
build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d \
  --game-debug --race-render-smoke-test --smoke-test-frames=240
```
