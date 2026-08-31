# Original vehicle physics revision

This follow-up audits the active Apple Silicon/Jolt path against the
authoritative Windows implementation in `GameCar.cpp`, `GameCar.h`,
`Player.cpp`, `HumanPlayer.cpp`, `DataBase.cpp` and `Physx.cpp`. It corrects
game-rule differences in the adapter; it does not retune the car by eye.

## Corrected source semantics

| Area | Windows/PhysX behavior | Corrected portable behavior |
| --- | --- | --- |
| Motor torque | `CarMotorDesc::CalcTorque` is written in full to every `lead` wheel | Jolt engine propagation is disengaged and the same full torque is applied to every driven `WheelWV` |
| Transmission | Source gears are `-1` neutral, `0` reverse and `1..5` forward; RPM is calculated from the first driven wheel before shifting | The adapter owns the same gear state, ratios, RPM cap and `maxRPM / 1.8` / `maxRPM` automatic thresholds |
| Reverse | `mcBack` brakes forward motion, then selects gear 0 and applies negative torque | Player brake input has a distinct `reverse` command; AI `brake` remains brake-only |
| Brakes | `brakeTorque` and `restTorque` are per-wheel values | Full braking is preserved; only powered low-speed `restTorque` is omitted at the Jolt boundary because Jolt interprets it as a wheel lock |
| Workshop tires | `tireSpring` is an overload cutoff in `MyContactModify`, not suspension stiffness | Workshop values no longer alter the coil spring; tire impulse is capped at 1.5g or released above the source cutoff |
| Tire curves | `NxTireFunctionDesc::hermiteEval` supplies two cubic zero-tangent pieces; within `NX_SKIN_WIDTH=0.025` of the preceding contact point clamped wheels instead use static `mu=extremumValue` | The max-impulse callback evaluates the exact source cubic outside the threshold and preserves source-step contact history for the static branch; ground friction and stiffness are not multiplied in |
| Wheel inertia | `inverseWheelMass` is inverse axle rotational inertia | Jolt `mInertia` is `1 / inverseWheelMass`, without a synthetic cylinder/radius conversion |
| Air behavior | With no wheel contact `JumpProgress` adds a second gravity acceleration and local pitch acceleration | The fixed-step adapter applies the same extra gravity and `flyYTorque` |
| Stabilization | Local angular momentum uses `angDamping`; airborne roll/pitch momentum and pose are clamped | Equivalent local-axis damping and clamp limits run each fixed step |
| Steering | `steerRot` requires a driven-wheel contact, or any contact when `gravEngine` is set | Contact eligibility, rear-wheel pivot and source yaw correction match that branch |
| Clutch | Oil disables tire reaction; selected cars ignore it through `clutchImmunity` | Traction suppression and immunity are both preserved; immune cars do not receive the oil spin |
| Coordinates | Linear and angular vectors transform differently across the reflected Z-up/Y-up basis | Angular velocity now uses the axial-vector sign transform |
| Materials | Scene material 0 is friction/restitution 0.5; cars use 0.08 or 0.02 primary and 2.0 secondary friction with `MIN`; track uses 0.1/`AVERAGE`, border uses dynamic 4.0/`MAX`; restitution always combines with `AVERAGE` | Jolt bodies carry the same coefficients, global callbacks reproduce PhysX combine-mode priority, and car contacts retain two-direction anisotropy |
| Collision groups | `Physx.cpp::Scene` disables 13 exact group pairs among `cdgShot`, `cdgShotBorder`, `cdgShotTrack`, `cdgShotTransparency`, `cdgWheel` and `cdgTrackPlane` | Jolt object layers encode the original group independently of motion type and apply the same symmetric 8-group matrix; weapon preparation selects the source group |
| Shape skin/solver | `Physx.cpp` sets `NX_SKIN_WIDTH=0.025`; `LoadCrushObj` raises movable crush boxes to `0.1`; `NxBodyDesc` uses four solver iterations | Jolt uses `0.05` penetration slop for a pair of default skins and four velocity iterations; the excess `0.075` of each explicit crush skin is represented by a per-shape collision inset |
| Body sleep | `DataBase::AddPxBody` sets mass-normalized `sleepEnergyThreshold=0.05`; `NX_SLEEP_INTERVAL` is `0.4 s` | Jolt's representative-point movement test uses the equivalent enclosing-sphere radius rate `0.5*sqrt(2*0.05)` and the same `0.4 s`; awake state is exported for cars, decorations and debris |
| Wheel queries | Wheels do not collide with shot-transparent borders or other cars | Suspension raycasts reject border and vehicle bodies |
| Reset | Pose, velocities, gear and wheel state are reset | Wheel angular/rotation/steer state, neutral gear and idle RPM are explicitly restored |

Additional serialized fields now loaded from every original `ctCar` record are
`angDamping`, `autoGear`, `flyYTorque`, `clampXTorque`, `clampYTorque`,
`gravEngine`, `clutchImmunity`, `maxSpeed`, `tireSpring` and the body material
index. Body `sleepEnergyThreshold` is also preserved through cars, movable
decorations and detached pieces. `tireSpring` supplied by the workshop is accumulated separately from
the suspension spring.

## Validation

`--physics-smoke-test` now uses a large isolated source-data floor for
drivetrain measurements, so stationary grid opponents or nearby map geometry
cannot hide a motor defect. It checks:

- idle RPM, forward acceleration and full driven-wheel propagation;
- clutch slip and rolling non-driven wheels;
- braking, steering and transmission state;
- `mcBack`, reverse travel and the brake-to-forward transition;
- reset of gear and every wheel angular velocity;
- extra airborne gravity and pitch acceleration;
- simultaneous acceleration/contact state for all racers selected from the
  original tournament data;
- border contact metadata/force, debris bodies and respawn lifecycle.
- both sides of the source mass-normalized sleep-energy boundary and the
  exact `0.4 s` wake-counter interval.
- material-0 restitution, Average/Min/Max combine priority, anisotropic car
  contacts and the PhysX `2 m/s` minimum bounce speed.
- all 64 directed source collision-group pairs, real Shot/ShotBorder/ShotTrack
  contacts with the track, and the wheel/shot-transparent exclusion.
- permanent `NX_IGNORE_PAIR` filtering for a projectile/effect and its exact
  source car actor, including a returned Thunder crossing the owner again.

Representative final acceptance also runs tracks 0, 16, 48 and 64 through the
packaged arm64 Debug executable and a 240-frame bgfx/Metal race integration
smoke.

## Backend boundary

Jolt and PhysX 2.8.4 use different suspension/contact solvers, so trajectories
cannot be bit-identical. The adapter preserves the source two-direction track
contact, scalar coefficients, combine mode, tire curves, shape skin distances,
iteration count, impulse cutoff and game-side state machine. Constraint order
and the inner contact implementation remain Jolt-specific. Jolt has no
per-body energy sleeper, so its point-movement test is calibrated to the
uniform `0.05` threshold used by all 95 persistent dynamic bodies in the
shipped database; short-lived projectile sensors retain their source `0.005`
provenance but never sleep while acting as sensors.

`eff9338:Weapon.cpp::Proj::PrepareProj` and
`GameBase.cpp::DeathEffect::OnDeath` also install permanent PhysX
`NX_IGNORE_PAIR` flags. The earlier portable runtime instead re-enabled owner
contact once a projectile left a coarse vehicle box. That invented
`ownerCollisionArmed` state is removed: source `ignoreContactProj` and
`effectPxIgnoreSenderCar` now resolve the one excluded car actor to a vehicle
index carried by `ProjectileBodyDescription`. Jolt filters only that pair;
track, border, decorations and other vehicles keep their normal contacts.
