# Remaining work — macOS port

State of the `macos-port` branch. Written to be honest about what is proven
versus what merely compiles, because on this port those are very different
things.

## Where it stands

| Target | LOC | Status |
|---|---|---|
| `XPlatform` | ~700 | **builds** (arm64) |
| `MathLib` | ~7,000 | **builds** |
| `LexStd` | ~7,000 | **builds** |
| `NetLib` | ~5,000 | **builds** — arm64 dylib |
| `Tests` | ~350 | **builds**, 50 runtime checks pass |
| `Rock3dEngine` | ~35,500 | **builds** |
| `Rock3dGame` | ~73,600 | 10 of 52 translation units compile |
| `RRR3d` (the exe) | ~350 | not attempted |
| `MapEditor` | ~4,600 | excluded from non-Windows builds (MFC) |

**Nothing renders and nothing runs.** Five libraries compile and their
primitives are tested. That is a long way from a game.

## What is actually verified

- The CP1251 → UTF-8 transcode round-trips byte-exactly, all 139 files.
- The 32 D3DX math functions compute correct results — 50 runtime checks,
  mutation-tested to confirm they fail when the code is wrong.
- The `XPlatform` Win32 substitutes work: recursive critical sections,
  auto/manual reset events, `WAIT_TIMEOUT`, cross-thread signalling, the
  thread pool, non-ASCII UTF-8 round trips.
- All 48 shader entry points compile DXSO → SPIR-V → MSL → metallib
  (`tools/shader-pipeline/`).
- PhysX 4.1 builds as arm64 static libraries (`tools/setup-physx-macos.sh`).

## What is NOT verified

- **The MSVC build.** Unverified since commit 12. CI caught a real regression
  once already (`d3dx9_compat.h` including only `d3d9types.h`, which does not
  declare `D3D_OK`). Every commit since then is unchecked on Windows, and
  `Rock3dEngine` specifically is *known broken* there — see below.
- **Any rendering.** Shaders compile; none has been executed on a GPU.
- **Any physics.** No PhysX call has been made at runtime.
- Shader coverage is the **default macro permutation only** — every `#if`
  false. The game drives each `.fx` through `Shader::MacroBlock` with many
  different `#define` sets, so real coverage is a multiple of 48.

---

## 1. Finish the PhysX migration

**`Rock3dEngine` builds.** The engine-side migration is done: actors, shapes,
scenes, contact callbacks, cooking and the collision-group table are all on
PhysX 4.1.

`Rock3dGame` is down from 336 `Nx*` references to 90, concentrated in
`GameCar.cpp` (26), `Weapon.cpp` (15) and `DataBase.cpp` (12). What is left is
the part with no mechanical equivalent — the wheel runtime API, raycast
queries, contact-stream iteration and material indices — plus the vehicle model
itself.

Closed since this document was written:

- **Momentum accessors.** `px::GetLinearMomentum` and friends reproduce the 2.8
  definitions, including angular momentum as `R·I·Rᵀ·ω` rather than
  `mass × ω` — the latter would be wrong for every body with anisotropic
  inertia, which is all of them.
- **`NX_AF_LOCK_COM`** now drives `setCMassLocalPose`.

Historical note on the table below: it was written when the header failed
first, which stopped compilation before the rest of the file was parsed. The
"221 errors, all in `px/Physx.h`" figure was an artifact of that. Treat error
counts as a floor until the file compiles.

Remaining `Nx*` symbols at the time of writing:

| Symbol | Count | Becomes |
|---|---|---|
| `NxActor`, `NxActorDesc`, `NxBodyDesc` | 22 | `PxRigidDynamic` / `PxRigidStatic`, created directly |
| `NxTireFunctionDesc`, `NxWheelShape(Desc)`, `NxSpringDesc` | 27 | `PxVehicle` + a custom tire force shader |
| `Nx*Shape`, `Nx*ShapeDesc` (Box/Sphere/Capsule/Plane/Convex/TriangleMesh) | 25 | leftover accessors; the geometries are already migrated |
| `NxUserContactModify`, `NxUserContactReport`, `NxUserNotify` | 4 | `PxContactModifyCallback`, `PxSimulationEventCallback` |
| `NxContactPair`, `NxContactCallbackData`, `NxConstContactStream` | 5 | `PxContactPair`, contact stream iterators |
| `NxSceneDesc` | 1 | `PxSceneDesc` |

Split this in two and do not let them blur:

**1a — make it compile.** Actor type, static/dynamic creation (the existing
`_body != nullptr` check already discriminates), shape attachment via
`PxRigidActorExt::createExclusiveShape`. Gets `Rock3dEngine` building, which
is the prerequisite for testing anything.

**1b — make it correct.** Everything below compiles fine when wrong.

### Behaviour gaps currently open

These are recorded here because the code builds without them and the game
would simply behave wrong:

- **`Shape::_density` is orphaned.** Serialised and settable, but no longer
  reaches the physics — `AssignToDesc` was its only consumer. Belongs in
  `PxRigidBodyExt::updateMassAndInertia`. Until then every body gets default
  mass.
- **`Shape::_materialIndex` is orphaned**, the same way. It should select the
  `PxMaterial` passed to `createExclusiveShape`. Until then everything gets
  default friction. Needs a `Manager`-owned index-to-`PxMaterial` map fed from
  whatever populated the 2.8 table.
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
- **Wheels have no suspension and generate no tire force.** The placeholder
  geometry is a sphere with `eSIMULATION_SHAPE` cleared so it cannot collide
  in the meantime.
- **Plane shapes now move with their actor.** `PxPlaneGeometry` has no normal
  or distance, so the equation lives in the local pose. Every plane in this
  game is on a static actor, so this is currently inert.
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

## 2. Get `Rock3dGame` compiling

73,600 LOC, untouched. Expect the same conformance patterns already fixed a
dozen times — dependent-base lookup, `address of temporary`, `friend class`
not introducing names, MSVC-only STL. Plus:

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
2.7.1 for Wine. Its `d3d9fe/` is ~13k lines of the same mapping and
`docs/METAL-BACKEND-NOTES.md` records the decisions. The Wine coupling
(`winemetal` bridge, unixlib) is what a native port drops. Unresolved: it
never rendered 3D, and whether that was a backend bug or an artefact of the
Wine boundary is not established.

## 4. D3DX is ours regardless

Not part of D3D9, so no graphics backend provides it:

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
  by assumption.
- `Stream.h`'s `MemoryWriteBuffer`/`MemoryReadBuffer` are Windows-only now;
  they go with the PhysX 4.1 move above.
- `d3dx9math.h`/`.inl` carry four documented deviations from upstream Wine.
  Two are SDK-parity fixes (`D3DXPlaneDotCoord`/`DotNormal` typed
  `D3DXVECTOR4*` by both Wine and MinGW, `D3DXVECTOR3*` by the SDK); two
  restore constructors the previous maintainers had added to the vendored
  Microsoft header.

## Suggested order

**Target milestone: the whole tree compiles and links** — an executable that
starts and fails at device creation. Audio, video and input get stubs, not
ports, until that holds. The point is to surface unknown blockers in the
73,600 untouched lines early rather than after the physics is perfect.

1. **PhysX 1a** — compile only. Unblocks `Rock3dEngine`.
2. **Move Windows to PhysX 4.1** — see below.
3. **Write physics tests**, now that the engine builds.
4. **PhysX 1b** — density, material, momentum, callbacks, then the vehicle
   model, test-first.
5. `Rock3dGame`: conformance pass, audio/video/input **stubbed**.
6. `RRR3d` links. Milestone reached.
7. Then, in either order: the Metal backend against
   `docs/macos-graphics-backend.md`, and D3DX (effects, textures, fonts).
8. Un-stub audio (FAudio), input (SDL_GameController), video.

Steps 1–6 are needed for Linux as well as macOS, and are largely mechanical.
Step 7 is where the genuine unknowns are.
