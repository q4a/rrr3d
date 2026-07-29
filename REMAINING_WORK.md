# Remaining work — macOS port

State of the `macos-port` branch. Written to be honest about what is proven
versus what merely compiles, because on this port those are very different
things.

## Where it stands

**The game runs.** It opens a window, plays its menus, loads the garage, and
starts and runs a race, at 60fps.

```
$ ./bin/Debug/RRR3d
```

Working end to end: window and event loop, keyboard and mouse, the whole 2D
interface, DDS/PNG/JPG textures, text, the effects runtime compiling all 23
`.fx` files, the 3D scene renderer, PhysX simulation, and a race that runs to
completion.

| Target | LOC | Status |
|---|---|---|
| `XPlatform` | ~10,400 | **shared library** — Win32 substitutes, D3DX, vendored headers |
| `MetalBridge` | ~4,900 | **works** — the winemetal ABI over Metal |
| `D3D9Metal` | ~40,000 | **works** — vendored DXVK D3D9 front-end + d9mt Metal backend |
| `TinyXml` | ~5,800 | builds — vendored TinyXML 2.5.3 |
| `MathLib` | ~4,900 | builds |
| `LexStd` | ~7,100 | builds |
| `NetLib` | ~5,100 | builds |
| `Tests` | ~360 | builds, 50 runtime checks pass |
| `Rock3dEngine` | ~36,100 | builds and runs |
| `Rock3dGame` | ~73,800 | builds and runs |
| `RRR3d` | ~700 | builds, links, runs, plays |
| `D3D9Triangle` | ~130 | backend smoke test |
| `BridgeTriangle` | ~300 | layer-by-layer bridge harness |
| `MapEditor` | ~4,600 | excluded from non-Windows builds (MFC) |

## What is verified

- **A race runs.** Menus, garage, character select, a race that starts and
  keeps running.
- **The graphics stack draws**, from `Direct3DCreate9` through DXVK, d9mt,
  MetalBridge, to Metal, presenting through a CAMetalLayer in an SDL window.
- **All 23 `.fx` files compile at runtime** through vkd3d-shader, across the
  macro permutations `Shader::MacroBlock` drives — not just the default
  permutation, which is all the offline check covered.
- **Every texture the game ships loads**: 312 DDS across DXT1/3/5 and
  uncompressed, 6 of them cubemaps, plus 213 PNG and 2 JPG.
- **Text renders** through a real `ID3DXFont` over stb_truetype.
- **Sprites blend.** Menus, character select and the garage draw their masked
  art correctly — curved panels, the cursor and the HUD frames. This was a
  defect until the effects runtime was made to restore the shaders it binds;
  see lesson 7.
- **A race renders, with the post-processing chain on.** Track, cars, water
  reflections, environment maps on bodywork, explosions, HDR and tone mapping.
  This was three defects that turned out to be one; see lesson 9.
- The bridge harness passes at every layer: plain draws, vertex descriptors,
  argument buffers, `newBufferWithBytesNoCopy`, function-constant
  specialisation, and blending.
- The CP1251 → UTF-8 transcode round-trips byte-exactly, all 139 files.
- The 32 D3DX math functions compute correct results, mutation-tested.
- PhysX 4.1 builds and links as arm64 static libraries.

## What is NOT verified

- **The MSVC build.** Unverified since commit 12, and `Rock3dEngine` cannot
  compile on Windows until it moves to PhysX 4.1 — see section 5.
- **Physics correctness.** The simulation runs; cars do not drive
  (see defects 1 and 2).
- **Audio, video and input** remain stubs (section 6).

## The known defects, in the order they hurt

### 1. Cars do not drive

`RRR3D_VEHICLE=1` builds a real `PxVehicleNoDrive` per car. Everything in the
chain works and the car still does not go anywhere.

What is measured and true:

| | |
|---|---|
| timing | `dt = 0.0167`, one physics step per frame, `pause=0`, `startRace=1` |
| wheels on the road | `groundBelowWheel = 0.336` against `radius = 0.340` |
| suspension | carrying — spring force ≈ 10 kN against a rest load of 10230 |
| wheels rotate | omega to 6000, gears shift, RPM real |
| tire force | 54 kN longitudinal computed |
| geometry | sound — wheel bottom sits 65 mm below the chassis bottom |
| **the car moves** | **no** — creeps to ~2.5 m/s, then the AI resets it |

54 kN on 2000 kg is 27 m/s². The forces exist and the body does not respond to
them, and that is the whole remaining question.

**The teleporting is a symptom, not a cause.** `AICar::UpdateResetCar` declares
a car blocked below `cMaxSpeedBlocking` and `Player::ResetCar` teleports it to a
track node with its velocity zeroed. Identical to `main`. Several cars stuck at
similar progress get reset to nearby nodes, which is why they appear to jump
onto one another. Fix the movement and this goes with it.

**Do projectiles move?** A laser fired from a stationary car appears to hang in
front of it. If that reproduces with `RRR3D_VEHICLE` unset, the fault is not in
the vehicle model at all and everything above is downstream of something much
more basic. This is the single cheapest question left and it should be asked
first.

**Suspect list, in order:** the two `setLinearVelocity(NullVector)` call sites
(`Race.cpp`, `Player.cpp`) — both look like legitimate one-shot placement but
neither has been observed at runtime; whether `setWheelShapeMapping` indices
address what they are believed to; and `GameCar::StabilizeForce`, which was
tuned against a car with no suspension and now runs alongside one that has it.

### 2. The car's position is wrong during a race

The vehicle model — see section 5, where it has been the identified
highest-risk item since the original plan.

### 3. The `GPUSync` spin

`Engine::GPUSync` sits in `while (GetData(...) == S_FALSE);`, a spin with no
yield that measures at **~88% of the main thread**, and deliberately holds a
frame back.

It is skipped when `_frameLatencyOk` is set, which needs
`SetMaximumFrameLatency`, which needs a D3D9Ex device, which DXVK offers. That
was tried and reverted: an Ex device forbids `D3DPOOL_MANAGED`, which this
engine reaches through `mpManaged`, and managed resources here are locked and
written directly — the Ex equivalent cannot be locked at all. It fails at the
first texture. Fixing it means reworking texture upload through staging
surfaces, or bounding the frame queue another way. Reasoning is recorded at
the call site.

## Debugging this port

Two harnesses bracket the graphics stack, because a 130k-line game is a poor
place to ask which layer broke:

```
bin/Debug/D3D9Triangle              a triangle through the whole stack
bin/Debug/BridgeTriangle <mode>     through MetalBridge alone
```

`BridgeTriangle`'s modes add one layer at a time — `vertexid`, `stagein`,
`argbuf`, `nocopy`, `speccnst`, `blend` — and each renders offscreen and reads
the pixels back, so neither needs a window or a human. That matters: **a
CAMetalLayer's contents never appear in `screencapture`**, so there is no way
to see what the game drew from outside the process.

Which is what these are for:

```
RRR3D_TRACE=1              ABI-boundary tracing to rrr3d-trace.log
RRR3D_DUMP_FRAME=<n>       back buffer to frame.tga on GUI frame n
RRR3D_DUMP_SCENE=<n>       back buffer to frame3d.tga on 3D frame n
RRR3D_NO_HDR=1             skip the HDR pass and tone mapping
RRR3D_AUTORACE=<planet>    skip the menus and start a race on that planet
RRR3D_SCENE_CLEAR=1        clear the scene magenta and the back buffer green
RRR3D_FORCE_ALPHATEST=1    discard low-alpha GUI fragments
RRR3D_VEHICLE=1            build a PxVehicleNoDrive per car (see defect 1)
RRR3D_TIRE_STIFFNESS=<k>   tire force per unit slip per unit load
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1
```

The physics harness is the third of these, and the only one that can answer a
question about a car in under a second:

```
bin/Debug/PhysicsHarness            a car on a plane, driven, with invariants
```

It is honest about one thing worth knowing before trusting it: it drives with
300 Nm where the game uses 12450, so it under-tests everything that only breaks
under real torque.

`RRR3D_AUTORACE` matters more than it looks: a race is seven menus deep, and
every graphics defect that only appears in one is otherwise expensive to look
at twice. `RRR3D_SCENE_CLEAR` is what cracked the black race screen — the scene
coming up green and turning black half a second later said the composite was
fine and something later in the frame was painting over it.

`RRR3D_TRACE=1` also reports two things worth knowing about the effects
runtime: `FXSAMPLER` names every effect sampler as it is bound, or logs it if
it cannot be, and `LUM` reads the HDR luminance chain back at each reduction
stage. Both are one line each and both would have found lesson 9 in a minute.

Metal's validation layer found the sampler-layout bug. It did **not** find the
stencil bug, because a stencil compare of Never is perfectly legal — the
command-sequence trace found that one, by diffing an encoder that drew against
one that did not.

## Lessons this port keeps repeating

Each of these cost hours and is worth a reviewer's attention, because they are
the shape of bug a Windows-to-anything port produces:

1. **A flag that gates the meaning of other fields.** `WMTStencilInfo::enabled`
   was ignored; the zeroed struct behind it meant "stencil compare Never", so
   the GPU discarded every fragment of every draw. The game rendered its clear
   colour and nothing else, at a steady 60fps, with no validation error.
2. **Enum underlying types are part of a struct's layout.** `WMTSamplerInfo`
   widened seven `uint8_t` enums to `uint32_t`, making it 48 bytes instead of
   32 and shifting every field after the first.
3. **Platform constants differ.** `RAND_MAX` is 32767 on MSVC and 2147483647
   on macOS, so `RAND_MAX + 1` overflowed to `INT_MIN` and `RandomRange`
   returned negative array indices. Same code, same inputs: `1` on Windows,
   `-2` here. It surfaced as a crash in AI weapon code seconds into a race.
4. **Static libraries duplicate their globals.** `XPlatform` was linked into
   both the executable and `libRock3dGame.dylib`; the SDL shell published the
   window size into one copy and `View::GetWndSize` read the other, so
   `ScreenToView` divided by zero and every click arrived as `INT_MAX`. It is
   now `SHARED`.
5. **`wchar_t` is not 16 bits.** The localisation loader cast UTF-16LE file
   bytes to `wchar_t*` and halved the length.
6. **Verify the premise before bisecting the plumbing.** Transparency was
   chased through the entire blend path before a harness showed blending was
   never at fault.
7. **A reimplemented API has to restore state, not just apply it.**
   `ID3DXEffect::Begin` promises to preserve device state and `End` to put it
   back. The port applied each pass's shaders and render states but only ever
   restored the render states, so a 3D pass left its pixel shader bound. Every
   later GUI draw — fixed-function, expecting no shader — ran that shader
   instead, which writes alpha 1 and made every sprite opaque. It looked like a
   texture-alpha bug for a long time, and the trail through PNG decode, the
   engine's pixel data, the staging copy and the Metal blit found alpha correct
   at every step, because it was. **The tell was that the main menu was fine and
   character select was not**: the menu draws no 3D scene, so nothing had bound
   a shader before it. A defect that depends on which screen you are on is a
   defect in what the previous screen left behind.
8. **An implicit integrator forgives tuning an explicit one will not.**
   `NxSpringDesc`: *"The spring is implicitly integrated, so even high spring and
   damper coefficients should be robust."* So 2.8's suspensions never had to be
   well damped, and are not — the damping ratio `c / (2·√(k·m))` across the
   shipped cars runs from 0.71 down to **0.06**. PxVehicle integrates
   explicitly, so the undamped ones ring, their tire load swings between a
   quarter and three times rest, and traction arrives and leaves several times a
   second. It reads as a grip problem and is not one. It is also *per car*,
   which is why some sit and bounce differently to others. The general shape:
   when a replacement solver integrates differently, data tuned against the old
   one is not merely a different feel, it can be unstable.
9. **A declaration that is not a constant is invisible to a constant table.**
   An `.fx` writes `texture diffTex;` and `sampler2D diffMap = sampler_state {
   Texture = diffTex; };`. The compiled shader's constant table lists `diffMap`
   and never `diffTex`, because a texture object is not a shader constant. The
   port built its parameter table from the constant table alone, so
   `GetParameterByName("diffTex")` returned NULL, every `SetTexture` the engine
   made was dropped without a word, and **not one effect sampler in the game was
   ever bound**. It looked like three separate defects — the scene black under
   tone mapping, no shadows, no environment maps — and it was one missing link.
   The scene still drew because the engine independently binds material textures
   to the same low stages; what broke was everything that does not go through a
   material. The lesson is the shape: a silent `SetTexture` that hits no
   parameter is indistinguishable from one that works, so the parameter table
   has to be built from what the source declares, not only from what the
   compiler emits.

## 5. PhysX: migrated, correctness open

**The migration is complete** — every call site in the engine and the game is
on PhysX 4.1, and the simulation runs. What is left is correctness.

Closed properly along the way: momentum (angular as `R·I·Rᵀ·ω`, not
`mass × ω`), kinetic energy, the material index table PhysX 3+ dropped,
collision groups and masks via `PxDefaultSimulationFilterShader`, actor pair
exceptions, and `NX_AF_LOCK_COM`.

### The vehicle model — the cause of defect 2

The highest-risk item in the port, and now the visible one. PhysX 3+ deleted
the built-in wheel shape, so this is a rewrite rather than a rename.

`PxVehicleTireData` uses lateral stiffness plus a friction-vs-slip graph;
2.8's `NxTireFunctionDesc` is an extremum/asymptote slip curve. There is no
parameter mapping, and **every car's tuning in `Data/Car/*Wheel.txt` and
`db.xml` is written in the old terms**.

The way out is `PxVehicleWheelsDynData::setTireForceShaderFunction`, which
takes a custom `PxVehicleComputeTireForce`. Port the 2.8 curve maths into it
and the existing tuning stays valid — handling preserved by construction
rather than by ear.

Note 2.8 capsules ran along **Y**, PhysX 3+ along **X**, and capsule height
changed from full to half. Both are handled in `CapsuleShape`; the same class
of trap is likely in the wheel work.

### Other behaviour gaps

All compile silently, which is why they are written down.

**Wide:**

- **Wheels never register ground contact.** `WheelShape::GetContact` reports
  none and `GetAxleSpeed` zero until the `PxVehicle` work lands. No tire
  trails, no slip effects, no engine RPM from axle speed. Reporting no contact
  beats fabricating one: every caller tests the return value.
- **Cars will climb walls they slide along.** `PxContactSet` has no friction
  orientation, so the 2.8 contact-modify callback's friction-basis rebuild is
  deleted rather than ported.
- **Anisotropic friction is gone.** PhysX 3 removed it. The car materials used
  it precisely so a car sliding along a wall would skid rather than climb.

**Narrower:** `Shape::_density` is orphaned; `NX_AF_DISABLE_RESPONSE` is
mapped but not applied; contact reports fire on every pair; contact
modification cannot reject a pair by return value; `sumFrictionForce` is
always zero; plane shapes move with their actor; `PxSetGroupCollisionFlag` is
global where 2.8 was per-scene.

### Windows moves to PhysX 4.1 as well

**Decided.** The class bodies in `Physx.h` carry 56 unguarded `Px*`
references, so the Windows build of `Rock3dEngine` cannot compile regardless
of the include branch. One backend serves all three platforms.

Consequences: `extern.7z` supplies 2.8.4 for MSVC and needs a 4.1 Windows
build; Windows physics behaviour changes when macOS's does, so capture any
comparison data from the 2.8 build *first*.

## 6. Audio, video and input

Stubbed, not ported, with the original API shape kept so each is a contained
change:

- **XAudio2 + X3DAudio** — `xaudio2.h`/`X3daudio.h` declare exactly what
  `snd/Audio.cpp` uses, silently. **FAudio** (zlib, in Homebrew) reimplements
  this API, so adopting it means implementing these interfaces rather than
  rewriting 2,900 lines. The largest untouched subsystem and the most
  noticeable absence.
- **DirectShow video** — `video::Player` reports `STATE_NO_GRAPH`, which
  `World::IsVideoMode()` already treats as no cutscene playing, so cutscenes
  skip rather than stall. Check whether the 2013-era `.avi` files decode at
  all before writing a player; transcoding may be cheaper.
- **XInput** — reports no controller, so the game falls back to the keyboard.
  `SDL_INIT_GAMEPAD` is already on but nothing is wired to it.
  SDL_GameController maps nearly one-for-one; `XInputGetKeystroke` is the
  exception, needing explicit edge detection.

## 7. Loose ends

- `MapEditor` is MFC and excluded. Realistically a rewrite or a permanent
  Windows-only target.
- Asset paths use backslashes throughout; normalised in one place,
  `lsl::GetAppFilePath`, rather than at the ~2,300 literals.
- TinyXML 2.5.3 is vendored because `extern.7z` ships only a prebuilt MSVC lib
  and Homebrew carries tinyxml2, a different API.
- Build prerequisites on macOS: `brew install libogg libvorbis sdl3`, plus
  `tools/setup-physx-macos.sh` and `tools/setup-vkd3d-macos.sh`.
- `d3dx9math.h`/`.inl` carry four documented deviations from upstream Wine.
- The `.fx` files are game data, not source, and stay CP1251. Only comments
  are non-ASCII and the preprocessor strips them; the repo's encoding checker
  covers `src/`, which does not own them.

## Suggested order

1. **Ask whether anything moves.** Fire a weapon with `RRR3D_VEHICLE` unset and
   watch the projectile. It costs one run and it halves the problem: a laser
   that hangs in the air says the fault is not in the vehicle model, and every
   hour spent inside `PxVehicle` after that is spent in the wrong place. This is
   first because it was asked far too late.
2. **The vehicle model** (defect 1) — the port's highest-risk item, and now
   judgeable, because a race renders and can be reached in one command.
3. **A second look at the rendering, now that it can be seen.** Every effect
   sampler was unbound until this session, so nothing that depends on one has
   ever been judged: shadow maps, environment maps, normal maps, water,
   refraction, sun shafts. They draw. Whether they draw *correctly* is
   unexamined, and the shadow-map path in particular binds two samplers.
4. **Audio** — the largest untouched subsystem, and self-contained.
5. **Move Windows to PhysX 4.1** — unblocks CI as a regression signal for
   everything since commit 12.
6. **Input** (SDL_GameController), **video**, and the `GPUSync` spin.

Items 2, 4, 5 and 6 serve Linux as much as macOS.
