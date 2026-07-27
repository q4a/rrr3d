# Remaining work — macOS port

State of the `macos-port` branch. Written to be honest about what is proven
versus what merely compiles, because on this port those are very different
things.

## Where it stands

**The tree builds and links.** `RRR3d` is an arm64 Mach-O that runs:

```
$ ./bin/Debug/RRR3d
rrr3d: no graphics backend on this platform yet -- Direct3DCreate9.
       See docs/macos-graphics-backend.md.
```

| Target | LOC | Status |
|---|---|---|
| `XPlatform` | ~9,800 | **builds** — Win32 substitutes plus vendored headers |
| `TinyXml` | ~5,800 | **builds** — vendored TinyXML 2.5.3 |
| `MathLib` | ~4,900 | **builds** |
| `LexStd` | ~7,100 | **builds** |
| `NetLib` | ~5,100 | **builds** |
| `Tests` | ~360 | **builds**, 50 runtime checks pass |
| `Rock3dEngine` | ~36,100 | **builds** |
| `Rock3dGame` | ~73,800 | **builds** — 52 of 52 translation units |
| `RRR3d` | ~420 | **builds and links** |
| `MapEditor` | ~4,600 | excluded from non-Windows builds (MFC) |

**Still nothing renders.** The game initialises and stops at device
creation, which is exactly what the graphics seam is for. That is a real
milestone and not a playable game.

## What is actually verified

- The CP1251 → UTF-8 transcode round-trips byte-exactly, all 139 files.
- The 32 D3DX math functions compute correct results — 50 runtime checks,
  mutation-tested to confirm they fail when the code is wrong.
- The `XPlatform` Win32 substitutes work: recursive critical sections,
  auto/manual reset events, `WAIT_TIMEOUT`, cross-thread signalling, the
  thread pool, non-ASCII UTF-8 round trips.
- All 48 shader entry points compile DXSO → SPIR-V → MSL → metallib
  (`tools/shader-pipeline/`).
- PhysX 4.1 builds and links as arm64 static libraries
  (`tools/setup-physx-macos.sh`).
- The vendored TinyXML sources match the vendored headers: the release's
  own headers are byte-identical to `extern/tinyxml/include` apart from
  line endings.

## What is NOT verified

- **The MSVC build.** Unverified since commit 12. CI caught a real regression
  once already (`d3dx9_compat.h` including only `d3d9types.h`, which does not
  declare `D3D_OK`). Everything since is unchecked on Windows, and the
  Windows physics build cannot pass until it moves to PhysX 4.1 — see below.
- **Any rendering.** Shaders compile; none has been executed on a GPU.
- **Any physics at runtime.** The engine links against PhysX 4.1 but no
  simulation step has ever run.
- Shader coverage is the **default macro permutation only** — every `#if`
  false. The game drives each `.fx` through `Shader::MacroBlock` with many
  different `#define` sets, so real coverage is a multiple of 48.

## The three things between here and a playtest

1. **The graphics backend.** Twelve entry points, listed in
   `src/XPlatform/source/d3d9_stub.cpp`. `Direct3DCreate9`/`Ex` is Direct3D
   itself; the other ten are D3DX, which no backend provides. See section 3.
2. **A window and a message loop.** `RRR3d.cpp`'s Win32 shell is compiled
   out and the placeholder `main()` has no event pump, so there is nothing
   to drive frames, input or resizing. SDL3 is the intended replacement.
3. **Audio and input are silent stubs.** See section 2.

Physics is no longer on this list, which is the change since the last
revision.

## 1. PhysX: done, with gaps

**The migration is complete.** Every PhysX call site in the engine and the
game is on PhysX 4.1: actors, shapes, scenes, cooking, contact callbacks,
collision filtering, materials, raycasts, contact streams, momentum and the
vehicle model. No `Nx*` type remains outside three project-local helpers in
`GameCar.cpp` that were only ever named by convention, and one block of
commented-out debug drawing.

What is left is correctness, not compilation — and none of it can be checked
until something runs. The gaps below are the whole of it.

Closed along the way, each properly rather than approximated:

- **Momentum.** `px::GetLinearMomentum` and friends reproduce the 2.8
  definitions, including angular momentum as `R·I·Rᵀ·ω` rather than
  `mass × ω`, which would be wrong for every body with anisotropic inertia.
- **Kinetic energy.** `px::ComputeKineticEnergy`, translational plus
  rotational, replacing `NxActor::computeKineticEnergy`.
- **The material table.** `px::Manager` keeps the index PhysX 3+ dropped, so
  `Shape::_materialIndex` means something again — it is serialised in saved
  games and the object database.
- **Collision groups and masks.** `PxDefaultSimulationFilterShader` reproduces
  2.8 filtering; the per-shape groups mask goes into the filter-data words the
  shader reads.
- **Actor pair exceptions.** `NX_IGNORE_PAIR` became a filter callback with a
  marker bit, so weapons still decline to collide with the car that fired
  them.
- **`NX_AF_LOCK_COM`** drives `setCMassLocalPose`.

### A note on error counts

Earlier revisions of this document quoted "221 errors, all in `px/Physx.h`".
That was an artifact of compilation stopping at the first fatal error in the
header, before the rest of the file was parsed. The true figure was higher and
spread wider. **Treat any error count as a floor until the file actually
compiles** — this bit twice.

### Behaviour gaps currently open

Ordered by how much they will change what a player sees. All of them compile
silently, which is why they are written down.

**Wide:**

- **Wheels never register ground contact.** `WheelShape::GetContact` reports
  none and `GetAxleSpeed` reports zero, because nothing computes them until
  the `PxVehicle` work lands. No tire trails, no slip effects, and no engine
  RPM derived from axle speed. Reporting no contact is deliberate over
  fabricating one: every caller tests the return value.
- **Cars will climb walls they slide along.** The 2.8 contact-modify callback
  rebuilt the friction *basis* per contact; `PxContactSet` has no friction
  orientation at all, so that is deleted rather than ported. Only the friction
  zeroing survives. Same concern as the next item.
- **Anisotropic friction is gone.** PhysX 3 removed it outright, so
  `staticFrictionV`, `dynamicFrictionV`, `dirOfAnisotropy` and
  `NX_MF_ANISOTROPIC` have no equivalent. The car materials used it precisely
  so a car sliding along a wall would skid rather than climb.

**Narrower:**

- **`Shape::_density` is orphaned.** Serialised and settable, but no longer
  reaches the physics — `AssignToDesc` was its only consumer. Belongs in
  `PxRigidBodyExt::updateMassAndInertia`. Until then every body takes its mass
  from `BodyDesc` alone.
- **`NX_AF_DISABLE_RESPONSE` has no equivalent.** Expressed by clearing
  `PxShapeFlag::eSIMULATION_SHAPE`. Mapping recorded in `px::BodyFlag`,
  not yet applied.
- **Contact reports fire on every pair.** 2.8 raised them per actor via
  `contactReportFlags`; PhysX 3+ wants the pair flags requested in the filter
  shader, before any actor is consulted, so the field is stored and serialised
  but no longer reaches the simulation.
- **Contact modification cannot reject a pair by return value.** 2.8 returned
  `false`; the equivalent is ignoring every contact in the set, which is what
  the callback now does.
- **`sumFrictionForce` is always zero**, and `sumNormalForce` is an impulse
  divided by the step. PhysX 3+ reports one per-point impulse combining both.
- **Plane shapes now move with their actor.** `PxPlaneGeometry` has no normal
  or distance, so the equation lives in the local pose. Every plane in this
  game is on a static actor, so this is currently inert.
- **`PxSetGroupCollisionFlag` and `PxSetFilterOps` are global** where the 2.8
  calls were per-scene. One scene today, so equivalent; a second would
  silently share the table.

**Cosmetic, but worth closing:**

- **`&temporary` is downgraded from an error, not fixed.** ~40 sites, an MSVC
  extension; clang materializes the temporary identically, so behaviour
  matches. Worth cleaning up once the port runs and can be tested.

### The vehicle model

The highest-risk item in the whole port. PhysX 3+ deleted the built-in wheel
shape, so this is a rewrite rather than a rename.

`PxVehicleTireData` uses lateral stiffness plus a friction-vs-slip graph;
PhysX 2.8's `NxTireFunctionDesc` is an extremum/asymptote slip curve. There is
no parameter mapping between them, and **every car's tuning in
`Data/Car/*Wheel.txt` and `db.xml` is written in the old terms**. A naive
migration means retuning every vehicle by feel against a game that does not
run yet.

The way out is `PxVehicleWheelsDynData::setTireForceShaderFunction`, which
takes a custom `PxVehicleComputeTireForce` callback. Port the 2.8 curve maths
into that and the existing tuning data stays valid — handling preserved by
construction rather than by ear.

Also note: PhysX 2.8 capsules ran along **Y**, PhysX 3+ along **X**, and
capsule height changed from full to half. Both are handled in
`CapsuleShape::CreateGeometry`/`ApplyToShape`; the same class of trap is
likely in the wheel work.

### Windows moves to PhysX 4.1 as well

**Decided.** `Physx.h` branches at the include — `NxPhysics.h` on Windows,
`PxPhysicsAPI.h` elsewhere — but the class bodies below it already carry 56
unguarded `Px*` references. The Windows build of `Rock3dEngine` is therefore
not merely unverified, it cannot compile, and no arrangement of the macOS work
changes that.

The alternative was `#ifdef`-ing every affected member so Windows kept 2.8.4.
Rejected: it makes `Physx.h` dual-API throughout and every future physics
change has to be written twice — the opposite of the goal.

So the include branch comes out and one backend serves all three platforms.
Consequences to handle:

- `extern.7z` supplies PhysX 2.8.4 for MSVC. Needs a 4.1 Windows build, or
  `tools/setup-physx-macos.sh` generalised to fetch and build per platform.
- Windows physics behaviour changes at the same moment macOS's does, so the
  2.8 build stops being available as a reference. Capture whatever comparison
  data is wanted from it *before* this lands.
- `Stream.h`'s `MemoryWriteBuffer`/`MemoryReadBuffer` exist only to serve the
  2.8 cooking API and get deleted here.

## 2. Audio, video and input

`Rock3dGame` compiles: all 52 translation units. The conformance patterns were
the same ones the engine hit — dependent-base lookup, address-of-temporary,
`friend class` not introducing names, in-class `static const` without a
definition, MSVC-only STL.

`Stream.h`'s `MemoryWriteBuffer`/`MemoryReadBuffer` are already deleted; the
cooking moved to `PxDefaultMemoryOutputStream` and nothing referenced them.

Audio, video and input are **stubbed, not ported** — the milestone is a tree
that compiles and links first. All three stubs keep the original API shape so
the real implementation is a contained change:

- **XAudio2 + X3DAudio** — `src/XPlatform/header/xaudio2.h` and `X3daudio.h`
  declare exactly what `snd/Audio.cpp` uses, with a silent implementation.
  **FAudio** (zlib, in Homebrew) reimplements precisely this API, so adopting
  it means implementing these interfaces rather than rewriting 2,900 lines.
- **DirectShow** video — `video.cpp` and `playback.cpp` compile out;
  `video::Player` reports `STATE_NO_GRAPH`, already the state
  `World::IsVideoMode()` treats as no cutscene playing, so cutscenes skip
  rather than stall. Check whether the 2013-era `.avi` files decode at all
  before writing a player; transcoding the assets may be cheaper.
- **XInput** → `src/XPlatform/header/xinput.h`, reporting no controller so the
  game falls back to the keyboard. SDL_GameController maps onto this nearly
  one-for-one. `XInputGetKeystroke` is the exception — SDL reports button
  state, not press/release/repeat, so it needs explicit edge detection.
- ~2,300 backslash separators in asset path literals, plus case sensitivity.

## 3. The graphics backend

Nothing implements the D3D9 declarations. This is the largest unknown.

**Chosen approach:** keep DXVK's D3D9 front-end, put a Metal backend behind
`DxvkContext`. See `docs/macos-graphics-backend.md` for the 62-method
contract, derived from DXVK v3.0.2 by intersecting the public `DxvkContext`
API with what `src/d3d9/` actually calls.

Why not the alternatives, with the evidence:

- **Stock DXVK over MoltenVK does not work.** DXVK requires `geometryShader`
  and `shaderCullDistance`; MoltenVK reports both unsupported because Metal
  has neither. Measured directly on an M1 Pro — everything *else* DXVK needs
  is now present (Vulkan 1.3.357, `timelineSemaphore`, `dualSrcBlend`), so
  this is not a maturity gap, it is architectural.
- **`d3dmetal-native`** is D3D11/12 only and x86_64-only.
- **`Gcenx/DXVK-macOS`** is pinned at DXVK 1.10.3 (2023), D3D10/11 only, and
  Wine-targeted.

A Metal backend never involves Vulkan, so none of those requirements apply.

`~/src/d9mt` is a prior D3D9-on-Metal attempt for this game against DXVK
2.7.1 for Wine. Its `src/d3d9fe/` is ~13k lines of the same mapping and
`docs/METAL-BACKEND-NOTES.md` records the decisions; there is also a `v2/`
tree worth reading before leaning on `src/`. The Wine coupling
(`winemetal` bridge, unixlib) is what a native port drops. Unresolved: it
never rendered 3D, and whether that was a backend bug or an artefact of the
Wine boundary is not established.

## 4. D3DX is ours regardless

Not part of D3D9, so no graphics backend provides it. All ten entry points
are declared in `src/XPlatform/source/d3d9_stub.cpp` beside the two Direct3D
ones, each failing rather than returning a half-built object:

- **`ID3DXEffect`** — 23 `.fx` files, ≤4 techniques each, exactly one pass
  each, no annotations. Bounded. MonoGame's MGFX, ReShade FX and Microsoft's
  FX11 are precedent, not drop-ins.
- **Texture loading** — `D3DXCreateTextureFromFileEx` and friends, 10 sites.
  stb_image plus a DDS/BCn parser. `Engine::d3dxUse(bool)` is an existing
  seam.
- **`ID3DXFont`** — **not** a debug-only concern. `graph::TextFont` is a
  first-class engine resource and every piece of UI text in the game renders
  through it. Needs a real text backend.

## 5. Loose ends

- `MapEditor` is MFC and excluded. Realistically a rewrite or a permanent
  Windows-only target.
- `Data/` paths use backslashes throughout and the game is case-insensitive
  by assumption. Untested — nothing has loaded an asset yet.
- TinyXML 2.5.3 is vendored in `src/TinyXml` because `extern.7z` ships only a
  prebuilt MSVC lib and Homebrew carries tinyxml2, a different API. Both
  encoding and include checkers treat it as vendored.
- `brew install libogg libvorbis` is now a build prerequisite on macOS.
- `d3dx9math.h`/`.inl` carry four documented deviations from upstream Wine.
  Two are SDK-parity fixes (`D3DXPlaneDotCoord`/`DotNormal` typed
  `D3DXVECTOR4*` by both Wine and MinGW, `D3DXVECTOR3*` by the SDK); two
  restore constructors the previous maintainers had added to the vendored
  Microsoft header.

## Suggested order

Steps 1–6 of the previous plan are **done**: the tree compiles and links and
the executable runs to device creation. What follows is the second half.

1. **Move Windows to PhysX 4.1.** Nothing else can be verified on Windows
   until this lands — `Rock3dEngine` cannot compile there. It also unblocks
   CI as a regression signal for everything since commit 12.
2. **Write physics tests.** Now possible: the engine builds and links, and
   `Tests` already has the harness. The behaviour gaps above are the list of
   what to write. Do this before touching the vehicle model.
3. **The graphics backend**, against `docs/macos-graphics-backend.md`. This
   is the long pole and the only genuine unknown left.
4. **D3DX** — effects, texture loading, `ID3DXFont`. Independent of step 3
   and can proceed in parallel; needed regardless of which backend wins.
5. **The SDL shell** — window, event loop, frame pump. Small next to steps 3
   and 4, but nothing is playable without it.
6. **Un-stub audio** (FAudio), **input** (SDL_GameController), **video**.
7. **PhysX correctness**, closing the behaviour gaps test-first, with the
   vehicle model last because it is the one that needs the game running to
   judge.

Steps 1, 2, 6 and 7 serve Linux as much as macOS. Steps 3 and 4 are where
the remaining risk lives.

## What would make a playtest possible

The shortest path, ignoring polish: steps 3, 4 and 5. Physics, audio and
input can all stay wrong or silent and the game would still be drivable
enough to judge whether the port is on course — which is the point of a
playtest. Everything else on this list can follow.
