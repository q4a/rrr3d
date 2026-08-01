# Remaining work — macOS port, attempt 2

Branch `macos-port-attempt-2`, based at `ec50208` (2021-06-14). 50 commits.

The full plan lives in the plan document; this file is the state of play and
the things that would be expensive to rediscover.

---

## Where it stands

| Phase | State |
|---|---|
| 0 — Backend trial | done: Bullet chosen, `docs/physics-backend-trial.md` |
| 1 — Hygiene, UTF-8, CI checkers | done |
| 2 — Build system, C++17 | done |
| 3 — XPlatform + D3DX math | done |
| 4 — PhysX 2.8 shim over Bullet | done |
| 5 — SDL3 shell | code done, **not verified end to end** (blocked on 7) |
| 6 — MetalBridge | done, all six test modes pass |
| 7 — D3D9 on Metal | **builds and runs; the triangle does not draw** |
| 8 — D3DX runtime, first pixels | not started, blocked on 7 |
| 9 — `NxWheelShape` | not started |
| 10 — Windows cutover | deferred by decision, not dropped |
| 11 — Audio, gamepad, video | not started |
| 12 — MapEditor on Dear ImGui | not started |

Everything compiles. `Rock3dGame`'s 55 translation units build and **zero `Nx`
symbols are undefined** — the shim carries the whole physics surface the game
calls. 27 undefined symbols remain across the tree: 12 D3DX/D3D9 (phases 7–8)
and 15 audio/video (phase 11).

### Test suites, all green

    bin/Debug/Tests            145 checks   XPlatform + D3DX math properties
    bin/Debug/PhysX28Tests       0 failures constants, conventions, no simulation
    bin/Debug/PhysX28Harness   460 checks   the shim against 2.8's specification
    bin/Debug/BridgeTriangle   6 modes      run from bin/Debug; takes a mode argument

`BridgeTriangle` modes: `vertexid stagein argbuf nocopy speccnst blend`. Each
adds one layer over the last; all render offscreen with pixel readback.

---

## The blocker: the D3D9 triangle does not draw

`bin/Debug/D3D9Triangle` connects the whole stack — `Direct3DCreate9` returns a
device, `CreateDevice` succeeds against a `CAMetalLayer`, the swapchain reports
800×600 A8R8G8B8 with D24X8 depth, `DrawPrimitiveUP` returns `S_OK`, 400 frames
Present, and `GetRenderTargetData` reads a surface back.

**Every pixel is exactly `D3DCOLOR_XRGB(40, 40, 90)` — the clear colour.**

### What has been ruled out

- **Not the readback.** The clear reaches the surface and is read back exactly,
  so the copy path works.
- **Not `Present` or `D3DSWAPEFFECT_DISCARD`.** The test originally read the
  back buffer *after* `Present`, where contents are undefined. That was a real
  bug; fixing it changed nothing.
- **Not the swapchain.** `./D3D9Triangle rtt` draws to a program-owned render
  target via `CreateRenderTarget`/`SetRenderTarget`, with no swapchain and no
  `Present`. Identical result.
- **Not dropped draws.** Temporary instrumentation in
  `DxvkContext::draw` (`extern/d9mt/src/d3d9fe/d9mt_context.cpp`, ~line 5254)
  showed the *first* draw is rejected by `commitGraphicsState` and later draws
  get past it and are encoded. `startRenderPass` binds successfully.

### The live hypothesis

The draw is encoded into a render pass whose results never reach the texture
that `GetRenderTargetData` copies. A Metal clear is a `loadAction` on a pass,
so the clear arriving while the draw does not suggests **two passes**: an
earlier one carrying the clear that gets committed, and the one carrying the
draw that is still open when the copy is taken.

### Next step

Read d9mt's `GetRenderTargetData` / readback path and check whether it ends the
open encoder and waits for the command buffer before copying. Compare against
how `startRenderPass` and `endEncoder` are sequenced around deferred clears in
`commitGraphicsState` (`d9mt_context.cpp` ~line 5166).

The plan's `RRR3D_SCENE_CLEAR` probe — paint render target and back buffer
different colours — is the reference branch's own tool for exactly this, and
has not been tried yet.

### Also unexplained

    libc++abi: terminating due to uncaught exception of type
    std::__1::system_error: mutex lock failed: Invalid argument

Once per run, after the work completes, so probably teardown. `EINVAL` from a
`std::mutex` lock usually means a destroyed mutex or reused storage. Rule it in
or out before trusting any timing-sensitive conclusion about the encoder.

---

## Getting a working tree

```sh
tools/setup-d3d9metal-macos.sh     # d9mt + its vendored DXVK into extern/
tools/setup-vkd3d-macos.sh         # libvkd3d-shader, needed by phase 8
tools/setup-boost.py               # Boost 1.69 headers for NetLib
cmake --preset=macos-arm64-debug
cmake --build build/macos-arm64-debug
```

`extern/` is gitignored. Every dependency comes from a committed script; if
something is missing, the script is the record, not this file.

Also required, via Homebrew: `sdl3`, `bullet`, `libogg`, `libvorbis`.

---

## Things that cost time to learn

**Read pixels, not exit codes.** `D3D9Triangle` prints `wrote tri.tga`, writes a
1.9 MB file and returns zero while drawing nothing. Every signal short of
reading the image says success.

**`grep -r` is `ugrep` on this machine and silently returns nothing.** Use `rg`.

**Error counts are a floor until a file actually compiles.** A fatal include
halts the count. The game went 7 → 21 → 240 → 120 → 12 → 39 → … as each fatal
include was cleared.

**d9mt carries its own DXVK.** `extern/d9mt/vendor/dxvk` is a documented v2.7.1
snapshot (`DXVK-VERSION` in that directory) with the Vulkan and SPIR-V
submodules already populated and four local patches applied. Fetching DXVK
separately puts two copies on the include path. There is one clone.

**The doubled include paths are correct.** `include/spirv/include/spirv/…` —
DXVK mounts the Khronos repos at directories named for what they provide, and
those repos already have an `include/` prefix.

**MSVC permissiveness is the dominant failure class**, in four flavours:
two-phase lookup (`this->`, `typename`), `friend class X;` not introducing `X`
into namespace scope (eleven occurrences), address-of-temporary passed to D3DX
(~60 sites), and two user-defined conversions in one sequence.

**When splitting one statement into two, check for a single-statement `if`.**
Doing this turned conditional `SendEvent` calls into unconditional ones three
times.

---

## Phase 4 — what the shim does and does not do

Implemented and tested against 2.8's specification, not against a feel target:
scene, actors, box/sphere/capsule/plane/mesh shapes, materials, collision
filtering, contact reports and the stream iterator, contact modification,
raycasts, triangle-mesh cooking, per-shape skin width, centre-of-mass offset.

Two `Unimplemented()` calls remain: `NxWheelShape` (phase 9, by design) and a
`setTiming` guard that fires only if `maxTimestep` drops below 1/60.

### Contracts that fail silently if broken

- **Material indices are handed out sequentially from 1**, with 0 the scene
  default. `db.xml` stores `materialIndex` 4 and 5 for track and border because
  `DataBase.cpp:4364-4397` creates them fifth and sixth. Index 3 never appears
  in shipped data, which independently confirms the ordering — the third
  material is `_nxWheelMaterial`, created and never assigned.
- **`getAngularMomentum` is `R·I·Rᵀ·ω`**, not `mass·ω`, and get∘set must be an
  exact identity: `GameCar::StabilizeForce` always ends with
  `setAngularMomentum`, so drift compounds permanently.
- **Skin width is per shape.** `db.xml` has 228 shapes at −1 (use the global
  0.025) and 69 at 0.1. A scene-wide value is wrong for a quarter of them.
- **Mass is computed once and never recomputed** when shapes change, because
  2.8 did not and `Actor::CreateNxShape` does exactly that.
- **`addLocalForce` applies at the centre of mass**, producing no torque. Every
  car sets a COM offset; applying at the actor origin invents torque.

### Verification style

Every scenario was checked with a deliberate mutation to prove it fails. Five
did their job: transposing both transform directions (round-trip alone cannot
see it), half-extents as full extents, dropping the material slot-0 reservation
(12 failures), asymmetric group matrix, and reporting impulse as force (0.8175
instead of 49.05 — exactly 1/60). In `src/Tests`, making critical sections
non-recursive **hangs** the suite, which is what the engine would do.

---

## Notes on the remaining phases

**Phase 8** needs `libvkd3d-shader` for `ID3DXEffect`, plus DDS/PNG/JPG loading
and `stb_truetype` for `ID3DXFont`. The traps are recorded in the plan; the
most expensive one on the reference branch was that `texture diffTex;` never
appears in a compiled constant table, so **no effect sampler was ever bound** —
which presented as three unrelated-looking rendering defects.

**Phase 11 — the plan is wrong about FAudio.** It ships no `xaudio2.h`; its
headers are `FAudio.h`, `F3DAudio.h`, `FACT*.h`, `FAPO*.h`, `FAudioFX.h`, and
it is a C API. Wine's xaudio2 DLL supplies the `IXAudio2` interfaces on top.
`src/XPlatform/header/xaudio2.h` already declares the surface `Audio.cpp` uses
— 2.7's shape, since the game calls `XAudio2Create` with a processor argument
and `GetDeviceDetails`, both removed in 2.8 — so phase 11 owes a C++ shim over
FAudio's functions, not just a link line.

**Phase 12** is smaller than it looks: the MFC engine embedding is one line
(`MapEditorView.cpp:151` sets `desc.handle`), which is the same mechanism the
SDL shell already uses.

**Phase 10 is deferred, not dropped.** Keep the `RRR3D_PHYSX28_SHIM` option and
the `if(MSVC)` branches.
