# M10 follow-up: rear-wheel rolling and World4 sky orientation

## Result

Two defects in the active original-data race path are fixed.

The player's non-driven rear wheels no longer receive the source
`restTorque` while throttle is applied. They roll with the road instead of
remaining locked and dragging the car. Explicit braking and the source
resting/coasting resistance remain active.

The original World4/hell cubemap is no longer displayed sideways. The bgfx
renderer now applies the same 90-degree X-axis left-handed-to-right-handed
coordinate conversion that legacy `SkyBox.cpp` applied before sampling the
original cube texture.

## Root causes

Legacy PhysX treated the car's 400 Nm `restTorque` as wheel resistance.
The Jolt adapter had converted it into a normalized brake input on every
frame, including frames with throttle. Jolt can use that brake input to lock a
stationary wheel, so the non-driven rear axle resisted the driven front axle.
The adapter now injects the resting brake only when no forward or reverse
throttle is requested.

The original D3D9 skybox transformed its left-handed source coordinates before
rendering. The Metal path loaded the correct `Data/World4/Texture/skyTex1.dds`
but omitted that transform, which rotated the image by 90 degrees. The
conversion is now part of the sky transform for every original cubemap.

## Regression coverage

`VehicleState` now exposes Jolt wheel angular velocities. The physics smoke
fails unless all contact wheels are represented, at least one driven wheel
rotates, and every contacting non-driven wheel rotates faster than 0.5 rad/s
under throttle. This directly covers the locked rear-wheel defect.

Verification performed on the native arm64 Debug bundle:

- incremental `macos-arm64-m10` build completed successfully;
- `--physics-smoke-test` passed the new rear-wheel rotation assertion plus
  acceleration, braking, steering, suspension, contacts and race state;
- World4 map1 `--weather=hell --race-render-smoke-test` passed at 240 and 1200
  frames through bgfx/Metal with six cars and four player wheel contacts;
- a captured World4 frame showed a horizontal horizon, bright lava at the
  bottom and the cloud layer above it.
