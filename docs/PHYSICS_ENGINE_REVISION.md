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
| Materials | Cars use 0.08 or 0.02 friction and zero restitution; track uses 0.1, border uses dynamic 4.0 | Source scalar coefficients and combine behavior are assigned by collision surface |
| Wheel queries | Wheels do not collide with shot-transparent borders or other cars | Suspension raycasts reject border and vehicle bodies |
| Reset | Pose, velocities, gear and wheel state are reset | Wheel angular/rotation/steer state, neutral gear and idle RPM are explicitly restored |

Additional serialized fields now loaded from every original `ctCar` record are
`angDamping`, `autoGear`, `flyYTorque`, `clampXTorque`, `clampYTorque`,
`gravEngine`, `clutchImmunity`, `maxSpeed`, `tireSpring` and the body material
index. `tireSpring` supplied by the workshop is accumulated separately from
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

Representative final acceptance also runs tracks 0, 16, 48 and 64 through the
packaged arm64 Debug executable and a 240-frame bgfx/Metal race integration
smoke.

## Backend boundary

Jolt and PhysX 2.8.4 use different suspension/contact solvers, so trajectories
cannot be bit-identical. Jolt exposes scalar body friction rather than the
two-direction PhysX anisotropic material used by the car body. The adapter
therefore preserves the source scalar coefficients, combine mode, tire curves,
impulse cutoff and game-side state machine, while solver iteration order
remains Jolt-specific.
