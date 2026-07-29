# Milestone 9.5: projected shadows, FxEmitter scheduling and Jolt calibration

## Result

M9.5 ports the three source-level gaps left after M9.4 into the active
bgfx/Metal and Jolt path. It does not introduce a second game runtime:

- `ShadowMapRender` now has its original two projected splits and 2048 map
  size;
- the portable particle renderer replays `FxEmitter` group scheduling instead
  of replacing ranges with average values;
- the Jolt adapter receives the original `CarMotorDesc` engine,
  transmission, rest-brake and steering rules.

No Windows/Parallels run is part of this milestone.

## Two-split projected shadows

The renderer abstraction now exposes a near and far shadow texture, matrix
and the split parameters. `RenderPass::Shadow` and `RenderPass::ShadowFar`
each render all source `gpShadowCast` actors into a 2048×2048 R32F/depth
target.

Split distance is copied from `ShadowMapRender::BuildViewProj`:

- camera near distance: 1;
- maximum shadow distance: 55 for isometric, 60 for third-person;
- logarithmic/uniform lambda: 0.1 for orthographic, 0.7 for perspective;
- two splits.

Both light projections are cropped to their actual scene-camera frustum
slice. The light-space crop is snapped to the 2048-texel grid to avoid
sub-pixel shimmer. The scene shader selects the cascade by view-space depth,
not by radial world distance.

The Metal fragment shader also ports the four depth comparisons and bilinear
interpolation from `ShadowMap.fx`: current texel, +X, +Y and +XY. This replaces
the previous single 1024 map and one hard comparison.

## Source FxEmitter schedule

The draw path now deterministically replays the source emitter state:

- a new start interval is sampled for every group;
- `density` is accumulated and only its integer part creates particles;
- the fractional remainder is retained for later groups;
- group life combines random `life` with indexed `rangeLife`;
- `startDuration`, unlimited emitters and `mnaWaitingFree` capacity are
  honored;
- source particle indices drive `rangePos`, `rangeScale`, `rangeRot` and
  `rangeLife` frames;
- distance-start emitters use reconstructed traveled distance;
- deferred transparent particle systems are stable-sorted by graph stage and
  back-to-front camera distance.

The existing 96 visible-particle safety limit remains at the final submission
boundary; it no longer changes source density/capacity calculations. FxTrail
continues to use real wheel-position history from M9.4.

## GameCar to Jolt calibration

The `ctCar` loader now preserves `motor/SEM`, `motor/steerSpeed` and
`motor/steerRot`, plus the non-serialized `CarMotorDesc` defaults:

- idle RPM 1000;
- rest brake torque 400;
- reverse ratio 1.5;
- forward ratios 2.66, 1.78, 1.30, 1.00 and 0.74;
- shift-down threshold `maxRPM / 1.8` and shift-up at `maxRPM`;
- immediate source gear change instead of Jolt's default clutch delays;
- flat source torque coefficient `SEM * 1.15`;
- keyboard steer ramp and direct analogue/AI steering;
- the source `steerRot` yaw correction around the rear wheel while a driven
  wheel has contact.

The rest brake is converted to a fraction of each original wheel's maximum
brake torque. The smoke test now also rejects an engine whose settled RPM does
not match `CarMotorDesc::idlingRPM`.

## Validation

The final arm64 Debug build and Metal shaders complete without new warnings.
`--physics-smoke-test` passes the original map1 collision/handling/state
matrix, including the new idle-RPM assertion.

The final 240-frame Metal matrix is:

| Track | Surface/effect path | Cube draws | Normal-map draws | FxTrail |
| --- | --- | ---: | ---: | ---: |
| World1 track 0 | fair/default | 42 | 0 | 168 |
| World2 track 16 | rainy/water | 48 | 768 | 120 |
| World5 track 48 | snow/planar reflection | 56 | 0 | 64 |
| World4 track 64 | hell/magma | 56 | 0 | 28 |

Every case verifies both shadow passes, all six cube faces, Scene,
HDR/adaptation/bloom/composite/HUD, six cars, four player wheel contacts,
both original camera modes and render-target resize. World2 additionally
verifies Water, Reflection and source normal maps; World5 verifies Reflection.

Bundle packaging now copies exactly the 1196 original catalog entries plus
four portable metadata/UI files. This prevents File Provider's untracked
` 2` copies from entering the application; the strict verifier reports 1200
files and rejects both missing and extra resources. Resource, physics and
240-frame Metal smoke tests pass against this packaged tree.

A manual run through `Single Player -> Tournament -> Start race` confirmed the
countdown, original HUD/mini-map, AI movement and wheel trails. The frame had
no enlarged wheel meshes or new rotating artifacts.

## Remaining backend differences

Jolt is not PhysX 2.8.4, so solver/contact results cannot be bit-identical.
Particle scheduling is reconstructed deterministically for render passes;
non-trail distance emitters approximate historical travel from source speed,
and submission still has a safety cap. Metal shadow depth precision and bias
also cannot be pixel-identical to D3D9. These boundaries are explicit and do
not replace source game data or game rules.

The next step is to rebuild Milestone 10 from the corrected M5–M9.5 path,
then run Debug/Release bundle verification and a clean-machine launch test.
