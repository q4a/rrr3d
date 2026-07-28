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
- The bridge harness passes at every layer: plain draws, vertex descriptors,
  argument buffers, `newBufferWithBytesNoCopy`, function-constant
  specialisation, and blending.
- The CP1251 → UTF-8 transcode round-trips byte-exactly, all 139 files.
- The 32 D3DX math functions compute correct results, mutation-tested.
- PhysX 4.1 builds and links as arm64 static libraries.

## What is NOT verified

- **The MSVC build.** Unverified since commit 12, and `Rock3dEngine` cannot
  compile on Windows until it moves to PhysX 4.1 — see section 5.
- **Physics correctness.** The simulation runs; the car's behaviour is wrong
  (see section 2).
- **Audio, video and input** remain stubs (section 3).

## The known defects, in the order they hurt

### 1. Tone mapping renders the scene black

`RRR3D_NO_HDR=1` works around it. It now gates two things, and the second is
the one that matters: `InitHDREff` in `GraphManager::SetGraphOption`, and
`_toneMap->Render` in `GraphManager::Render`.

**Gating only the first was never enough**, which is what made this look
mysterious for so long. Bloom and sun shafts each call `InitToneMap`, so
`_toneMapRef` stays non-zero and tone mapping ran anyway — reading and writing
the scene target with its HDR luminance input missing, and blacking the frame.
So "HDR is off" and "the scene is black" were both true at once. With tone
mapping actually skipped, a race renders: track, cars, guard rails, particles.

The menus were never affected because they take the direct-to-back-buffer leg
and never reach a post-processing pass at all. That is why this looked like a
3D-scene defect rather than a post-processing one.

The HDR chain itself still runs and is still wrong: the scene draws into an
`A16B16G16R16F` target, reduces 128×128 → 1×1 for average luminance, and tone
maps. Every pass executes with draws in it, and the output is black, so the
luminance coming out of that reduction is wrong rather than absent.

Ruled out: `log(0)`, which `Down3x3LumLog` guards with a `+0.0001` epsilon; and
the `A16B16G16R16F` scene target, which does round-trip — forcing it to
`A8R8G8B8` changed nothing while the scene was black.

Still to check: whether d9mt's unconditional fast-math MSL turns an
intermediate in the exp/log reduction into a NaN — its own notes warn about
exactly that, and a NaN average luminance would black the frame like this.
`AdaptLum` is worth reading closely too: it assigns a `tex2D` result to a
`float2` and clamps only `.x`, so `.y` carries the max-luminance channel
unclamped into the exposure calculation.

### 2. The car's position is wrong during a race

Untouched. This is the vehicle model — see section 5, where it has been the
identified highest-risk item since the original plan.

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
RRR3D_NO_HDR=1             skip the HDR pass and tone mapping (see defect 1)
RRR3D_AUTORACE=<planet>    skip the menus and start a race on that planet
RRR3D_SCENE_CLEAR=1        clear the scene magenta and the back buffer green
RRR3D_FORCE_ALPHATEST=1    discard low-alpha GUI fragments
MTL_DEBUG_LAYER=1 MTL_SHADER_VALIDATION=1
```

`RRR3D_AUTORACE` matters more than it looks: a race is seven menus deep, and
every graphics defect that only appears in one is otherwise expensive to look
at twice. `RRR3D_SCENE_CLEAR` is what cracked defect 1 — the scene coming up
green and turning black half a second later said the composite was fine and
something later in the frame was painting over it.

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

1. **HDR** (defect 1) — currently gated off; the game looks right without it,
   but it is one value in a working chain. Worth re-checking now that the
   effects runtime restores shaders, since the tone-mapping passes are effects
   and the reduction runs several of them back to back.
2. **The vehicle model** (defect 2) — the port's highest-risk item, and now
   judgeable because the game runs.
3. **Audio** — the largest untouched subsystem, and self-contained.
4. **Move Windows to PhysX 4.1** — unblocks CI as a regression signal for
   everything since commit 12.
5. **Input** (SDL_GameController), **video**, and the `GPUSync` spin.

Items 2, 3, 4 and 5 serve Linux as much as macOS.
