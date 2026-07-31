# Physics backend trial

The PhysX 2.8 shim needs something underneath it. This is the measured
comparison of the three candidates, run on macOS arm64 in July 2026.

Each engine was built and a harness run against it. The harnesses are in the
session scratchpad rather than the tree — they are throwaway, and what they
produced is below.

## The question that actually decides it

The shim's rule is that the game must not be able to tell it is not talking to
PhysX 2.8, and that no tuning constant gets adjusted to compensate. So the
backend that wins is the one needing the least interpretation — and there are
exactly two places where a backend can *close* a 2.8 gap rather than merely
avoid creating one:

- **Per-contact-point friction direction.** 2.8's `NX_CCC_LOCALORIENTATION0/1`
  let `GameCar::OnContactModify` rebuild the friction frame from the touched
  track triangle. PhysX 3+ deleted it.
- **Anisotropic friction.** `NX_MF_ANISOTROPIC` with `dirOfAnisotropy` and
  `staticFrictionV`/`dynamicFrictionV`. Both car materials use it, and it is
  what stops a car sliding along a wall from climbing it. PhysX 3+ deleted it.

## Results

| | PhysX 4.1.2 | Bullet 3.25 | Jolt 5.3.0 |
|---|---|---|---|
| Bodies are raw pointers, as `NxActor*` is | yes | yes | **no** — `BodyID` handle via `BodyInterface` |
| Shape mutable in place | yes | yes | **no** — immutable, rebuild and swap |
| Capsule axis matches 2.8 (Y) | **no** — X-axis, half-height | yes | yes |
| Contact manifold: point, normal, separation, impulse | yes | yes (measured) | yes |
| Touched triangle index on a mesh | yes | yes (measured) | yes |
| **Per-point friction direction** | **no** | **yes** | **no** — scalar, per manifold |
| **Anisotropic friction** | **no** | **yes (measured)** | **no** |
| Raycast with a caller-supplied predicate | yes | yes (measured) | yes |
| Convex hull + mesh from raw arrays, instanced at scales | yes | yes (measured) | yes |
| **Gaps against 2.8** | **3** | **0** | **4** |

## Decision: Bullet

It is the only one of the three with no capability gap against PhysX 2.8, and
the reasons are not close.

It closes both gaps the other two leave open. `btManifoldPoint` carries
`m_lateralFrictionDir1`/`m_lateralFrictionDir2`, which is what
`NX_CCC_LOCALORIENTATION0/1` needs. `btCollisionObject::setAnisotropicFriction`
takes a per-axis friction vector, which is `dirOfAnisotropy` plus
`staticFrictionV` in one call.

Anisotropic friction was verified behaviourally, not just as an API that
exists: a box launched at 10 m/s across a plane with friction 1.0 slid **3.19 m**
isotropic and **29.09 m** with friction relaxed along one axis.

Its API shape is also the closest to 2.8's, which is the difference between the
shim translating and the shim bridging. Bodies are raw pointers the caller owns,
with a `setUserPointer` slot matching `NxActor::userData`. Shapes mutate in
place. Capsules run along Y, which removes the quarter-turn every capsule would
have needed under PhysX and the full-versus-half height conversion with it.
Bullet is a contemporary of PhysX 2.8, and it shows.

## What this does not establish

Stated plainly, because the trial should not be read as more than it is.

**Q5 was proved as API, not as behaviour.** The contact callback fires, reports
the touched triangle index, and the friction-direction fields accept values. It
was not measured that the solver then applies friction along the axis given.
That matters less than it looks: `GameCar::OnContactModify` sets
`dynamicFriction0` and `staticFriction0` to zero unconditionally — the
velocity-dependent value it computes is commented out — so for this game the
basis chooses *which* tangent is frictionless while the magnitude is zero
either way. Q6 is the one that carries real weight, and Q6 was measured.

**The harnesses are asymmetric.** Bullet's exercises all nine questions;
PhysX's and Jolt's cover build, API shape and the two decisive capabilities.
The unexercised items are ones all three plainly support.

**Bullet's solver is considered softer than the other two**, and it is the least
actively maintained. Neither was measured here. The first matters less for this
port than it would elsewhere: `NxWheelShape` is being implemented from its own
2.8 specification as raycasts, a spring and a tire force — the solver is not
being asked for a vehicle model, only for rigid bodies, contacts and queries.

**Build friction, for the record.** Bullet installed from Homebrew in one
command. PhysX 4.1 was already built here and requires exactly one of `NDEBUG`
or `_DEBUG` to be defined or its headers refuse to compile. Jolt built cleanly
from source in about a minute, but aborts at startup unless the consumer defines
the same four `JPH_*` macros the library was built with — the harness crashed
with no output until they matched, which is a real hazard for a project that
would carry them in two build systems.
