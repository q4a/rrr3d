# Original game debug mode on macOS

The macOS port exposes the original Windows debug code through two explicit
runtime modes. Both are disabled during an ordinary launch, regardless of
whether the macOS application itself is a Debug or Release build.

- `--game-debug` enables instrumentation without changing the campaign.
- `--legacy-windows-debug` reproduces the platform-independent global
  `_DEBUG`/`DEBUG_PX` execution and implies `--game-debug`.

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

## Windows `_DEBUG` compatibility mode

Launch the scenario-compatible mode with:

```sh
build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d \
  --legacy-windows-debug
```

It ports the active, platform-independent preprocessor branches from the
Windows sources:

- `World::RunGame` calls the equivalent of `GameMode::Run(false)`, so the
  release startup/logo flow is skipped.
- The new-tournament `main/main_eng`, gamer-selection `intaria/intaria_eng`
  and newly unlocked planet intro movies are skipped. Final/ending movies
  are not behind `_DEBUG` in the source and remain enabled.
- Patagonis receives `Data/Map/debugTrack.r3dMap` as its first pass-1 track;
  Nho receives `Data/Map/World5/map0.r3dMap`. Those two tracks have 99 laps.
  The other 88 tracks keep their source lap counts.
- Race weather is forced to `ewClody` after campaign, network or command-line
  selection, matching `Race::StartRace`.
- `DEBUG_PX` starts at `cGoRace` immediately instead of entering
  `cGoRaceWait`; there is no countdown or initial control block.
- The human car gets the original debug AI controller. This does not add a
  racer, vehicle or name to the result table. `AIDebug` leaves it disabled
  initially and F7 toggles it.
- View switching follows the source cycle: Third Person, Isometric, Lights,
  IsoView, FreeView, then Third Person. These transient styles are not saved
  to `user.xml`. In Lights/IsoView/FreeView, W/A/S/D moves at the original
  20 units/second; holding the right mouse button rotates Lights/FreeView at
  the source 0.005 radians per mouse pixel.
- LAN discovery uses the Windows Debug 500 ms/250 ms timing instead of the
  Release 3000 ms/500 ms timing, and the system cursor remains visible.
- `PxWheelSlipEffect::OnProgress` and `PairPxContactEffect::OnContact` are
  inactive, exactly as required by their source `#if !_DEBUG` guards. Thus
  tire skid smoke/trails/sound and the global contact spark/sound are absent
  in this comparison mode; ordinary gameplay is unchanged.

The type-9 `PxWheelSlipEffect` guard is applied inside the source behavior,
not by removing serialized wheel owners or muting a renderer proxy. Its base
`EventEffect` progress and axle animation still run, matching `eff9338`.
This gate was restored after the global MapObj wheel-slip migration on
2026-09-07; headless session and copy/normal-mode regressions cover it.

The earlier description of a “debug roster” was imprecise. `_DEBUG` does not
increase `numAI`: the apparent extra `AIPlayer` owns the already existing
human `Player` and is used only by `AIDebug`.

## Platform-specific branches

The compatibility switch does not mechanically reproduce Windows engine
implementation details that have no Metal/Jolt equivalent: D3D9 resource
lazy-loading guards, Direct3D lost-device/font handling, the D3D scanline
frame limiter, MSVC exception wrappers and PhysX Visual Remote Debugger.
Their portable counterparts continue to operate normally. The D3D FPS text
is represented by the more detailed `--game-debug` overlay.

The shipped Windows executable is a Release build. Its F1-F3 non-retail
handlers may exist, but `AIDebug`, the physics HUD, F6/F7 control, PhysX VRD
and the other `_DEBUG` blocks were removed by the preprocessor. The complete
mode therefore cannot be enabled in that binary by a configuration switch;
the Windows sources must be rebuilt with the appropriate debug definitions.
For future parity work, build the Windows Debug configuration so `_DEBUG` is
defined; `Rock3dGame/header/stdafx.h` then also defines `DEBUG_PX`. Compare
that executable against macOS `--legacy-windows-debug`, not against the
ordinary macOS launch.

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

The global compatibility path is covered with:

```sh
build/macos-arm64-m10/Debug/RRR3d.app/Contents/MacOS/RRR3d \
  --legacy-windows-debug --race-render-smoke-test \
  --smoke-test-frames=240
```
