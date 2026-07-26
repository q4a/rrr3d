# Milestone 9.2: legacy renderer passes on bgfx/Metal

## Result

M9.2 ports the remaining race-rendering behavior selected for this stage. It
does not wrap D3D9 and does not add look-alike gameplay. The macOS race now
uses a fixed bgfx/Metal frame graph:

1. half-resolution planar-reflection target with a reflected camera, source
   clip plane and inverted culling;
2. 1024×1024 directional shadow map;
3. full-resolution RGBA16F scene target;
4. bright-pass extraction and separable Gaussian bloom at half resolution;
5. exposure/tone mapping to the back buffer;
6. the original HUD in the final overlay view.

Render targets are recreated with the drawable on resize. The renderer
abstraction exposes render-target and pass concepts without leaking bgfx,
Metal, Cocoa or D3D9 types into game code.

## Source mapping

The portable loader now reads the legacy `grActor` rendering flags from the
same `db.xml` records used by Windows:

- `graphLighting == glPlanarRefl` enables projective reflection sampling for
  that source object;
- `gpShadowCast` selects static shadow-map casters;
- original opaque, transparency and additive material modes select depth
  writes and blending;
- `goDefault` content is submitted before the deferred transparency layer,
  whose source visual nodes are sorted back-to-front;
- water uses the reflected scene target rather than a generated substitute.

The post-process uses each world's original `luminanceKey`,
`brightThreshold`, `gaussianScalar` and `exposure` values transferred by
`applyOriginalEnvironment`. The projected black car quad from M9.1 has been
removed; cars, wheels and source `gpShadowCast` map objects now render into
the shadow map.

## Particles and world effects

The resource particle path follows the legacy `FxEmitter`/`FxSpritesManager`
semantics:

- one creation action produces a density-sized group;
- life is sampled once per group and `maxNum` limits live particles;
- time- and distance-triggered emitters use the original ranges;
- position, scale velocity, acceleration, gravity, `worldCoordSys` and
  `autoRot` come from `db.xml`;
- `fxDirSpriteManager` uses the source fixed-direction billboard basis;
  ordinary sprites face the race camera and apply their turn angle;
- resource particle systems are deferred to the opacity layer.

Rain is no longer a generated blue quad field. M9.2 loads
`world\db\root\ctEffects\rain`, its `Effect\drop` material and emitter ranges
from the original database, and follows the camera as in
`Environment::ProcessScene`.

## Static Windows-dependency audit

The active M9.2 source closure was searched for D3D/Direct3D/D3DX,
`HWND`/`HINSTANCE`, Windows headers, XAudio/X3DAudio and PhysX/Nx tokens.
The only matches were:

- `src/RRR3d/RRR3d.cpp`, selected exclusively by the `WIN32` CMake branch;
- `ISceneControl.h`, reachable from legacy editor/game interfaces but not
  from the non-Windows `Rock3dGame` source list;
- comments in the portable renderer/menu specification.

The macOS target continues to compile `main_bgfx_original_menu.cpp`, the
portable `OriginalRace`/profile/session sources, the Jolt adapter and the
bgfx device. Legacy D3D9 graph, PhysX, Windows window entry point,
network/Steam/video and XAudio/X3DAudio implementations remain excluded.
They are preserved as the Windows implementation and are not silently
replaced by stubs in the race target.

## Verification

The final M9.2 verification is intentionally performed only after all code
and documentation changes:

```bash
cmake --preset macos-arm64-m9
cmake --build --preset macos-arm64-m9 --target RRR3d -j 8
build/macos-arm64-m9/Debug/RRR3d --physics-smoke-test
build/macos-arm64-m9/Debug/RRR3d --race-render-smoke-test
build/macos-arm64-m9/Debug/RRR3d --verify-resources
```

No Parallels/Windows A/B run is part of M9.2.

The final run completed successfully:

- configure and the arm64 Debug build generated and compiled all six new
  Metal shader binaries;
- physics/session smoke resolved 1175 original collision triangles and
  passed vehicle, countdown, checkpoint/lap/finish, weapon/damage, bonus and
  respawn checks;
- resource audit resolved 1196 files and the selected original race;
- the Cocoa `MainMenu2 -> Single Player` smoke reported `bgfx/Metal` and
  completed 240 race frames with speed 9.22333 and four wheel contacts;
- the executable is arm64 Mach-O, minimum macOS 13.0, with system framework
  dependencies only.

## Remaining backend differences

M9.2 ports the selected render passes and particle behavior, but does not
claim bit-identical D3D9 pixels. Jolt remains numerically different from
PhysX 2.8.4, SDL audio does not reproduce the full X3DAudio
cone/obstruction DSP matrix, and the original luminance-adaptation history is
represented by deterministic exposure/tone mapping rather than a D3D9
read-back chain.
