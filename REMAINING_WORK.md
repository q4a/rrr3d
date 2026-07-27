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
| `Rock3dEngine` | ~35,500 | 221 errors, **all in `px/Physx.h`** |
| `Rock3dGame` | ~73,600 | not attempted |
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
  declare `D3D_OK`). Every commit since then is unchecked on Windows.
- **Any rendering.** Shaders compile; none has been executed on a GPU.
- **Any physics.** No PhysX call has been made at runtime.
- Shader coverage is the **default macro permutation only** — every `#if`
  false. The game drives each `.fx` through `Shader::MacroBlock` with many
  different `#define` sets, so real coverage is a multiple of 48.

---

## 1. Finish the PhysX migration

The only thing blocking `Rock3dEngine`. Remaining `Nx*` symbols:

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
  default friction.
- **Momentum accessors are gone.** PhysX 3+ removed
  `getLinearMomentum`/`setLinearMomentum`/`getAngularMomentum`; there is only
  velocity and mass. Six call sites in `NetPlayer.cpp` and three in
  `Player.cpp`. Mechanically `momentum = mass × velocity`, but these are the
  **multiplayer sync path**, so an error changes netplay behaviour rather
  than failing to build.
- **`NX_AF_DISABLE_RESPONSE` has no equivalent.** Expressed by clearing
  `PxShapeFlag::eSIMULATION_SHAPE`. Mapping recorded in `px::BodyFlag`,
  not yet applied.

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

## 2. Get `Rock3dGame` compiling

73,600 LOC, untouched. Expect the same conformance patterns already fixed a
dozen times — dependent-base lookup, `address of temporary`, `friend class`
not introducing names, MSVC-only STL. Plus:

- **XAudio2 + X3DAudio** (~2,900 LOC) → **FAudio**, a zlib-licensed
  accuracy-focused reimplementation of exactly these APIs, SDL2-only
  dependency, macOS supported. Closest thing to a free win remaining.
- **DirectShow** video (~1,500 LOC) → ffmpeg, or make cutscenes skippable.
  Check first whether the 2013-era `.avi` files decode at all; transcoding
  the assets may be cheaper than the player.
- **XInput** → SDL_GameController. `XInputGetKeystroke` has no SDL analogue
  and needs explicit edge detection.
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
  delete them once Windows also moves to PhysX 4.1.
- `d3dx9math.h`/`.inl` carry four documented deviations from upstream Wine.
  Two are SDK-parity fixes (`D3DXPlaneDotCoord`/`DotNormal` typed
  `D3DXVECTOR4*` by both Wine and MinGW, `D3DXVECTOR3*` by the SDK); two
  restore constructors the previous maintainers had added to the vendored
  Microsoft header.

## Suggested order

1. **PhysX 1a** — compile only. Unblocks `Rock3dEngine`.
2. **Push and check CI.** 13+ commits of unverified Windows build.
3. **Write physics tests**, now that the engine builds.
4. **PhysX 1b** — density, material, momentum, callbacks, then the vehicle
   model, test-first.
5. `Rock3dGame`: conformance pass, then FAudio, then input.
6. The Metal backend, against `docs/macos-graphics-backend.md`.
7. D3DX: effects, textures, fonts.

Steps 1–5 are needed for Linux as well as macOS, and are largely mechanical.
Step 6 is where the genuine unknowns are.
