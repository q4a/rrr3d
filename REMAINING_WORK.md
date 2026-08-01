# Remaining work — macOS port, attempt 2

Branch `macos-port-attempt-2`, based at `ec50208` (2021-06-14). 52 commits.

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
| 5 — SDL3 shell | code done, **not verified end to end** (blocked on 8) |
| 6 — MetalBridge | done, all six test modes pass |
| 7 — D3D9 on Metal | done: the triangle draws, both paths verified by pixel |
| 8 — D3DX runtime, first pixels | not started, and now unblocked |
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
    bin/Debug/D3D9Triangle     2 modes      no argument = swapchain, `rtt` = own target

`BridgeTriangle` modes: `vertexid stagein argbuf nocopy speccnst blend`. Each
adds one layer over the last; all render offscreen with pixel readback.

`D3D9Triangle` checks the pixels itself and exits non-zero if the triangle is
missing; it also writes `tri.tga` / `tri_rtt.tga` to look at.

---

## Phase 7, and why the triangle appeared not to draw

`bin/Debug/D3D9Triangle` connects the whole stack — `Direct3DCreate9`,
`CreateDevice` against a `CAMetalLayer`, an 800×600 A8R8G8B8 swapchain with
D24X8 depth, `DrawPrimitiveUP`, `Present`, `GetRenderTargetData` — and both the
swapchain path and a program-owned render target now read back the triangle.
The centre pixel is `0xff794343`, which is the Gouraud interpolation of the
three vertex colours at that point to the byte.

**The cause was asynchronous pipeline compilation, and nothing reports it.**

d9mt mirrors dxvk-async: a pipeline state seen for the first time is handed to
a background worker pool and **the draw is skipped** until it is hot.
`getRenderPso` in `d9mt_context.cpp` says so plainly — *"pso stays 0 until the
worker finishes; the draw site skips until then"* — and `commitGraphicsState`
returns false, silently, with no log line and no HRESULT. The clear still lands,
because a Metal clear is a `loadAction` on the render pass rather than
something the draw carries. So the surface reads back as flat clear colour while
`DrawPrimitiveUP` returns `S_OK`.

For a game that is a reasonable trade: geometry pops in a frame or two late. For
a program that draws once and reads the result it is fatal, and it is invisible.

`D3D9Triangle` now sets `D9MT_ASYNC=0` before touching D3D9 (d9mt caches the
answer in a function-local static, so it has to be before the first use). The
variable is not overwritten if already set, so `D9MT_ASYNC=1 ./D3D9Triangle rtt`
still reproduces the original failure — and the pixel check catches it:

    rtt: FAIL centre (400,300) is 0xff28285a, want 0xff794343
         -- the clear colour, so the draw did not land

### The two crashes this uncovered, both one bug

The async PSO workers outlived static destruction. `PsoWorkers` is a
namespace-scope static in `d9mt_context.cpp`, so it is constructed before the
function-local statics of other translation units and destroyed after them; its
destructor joins the threads, and a worker still compiling during that join
touches storage that is already gone. Two victims were live:

- `d9mt_backend.h`'s `logf` mutex — `EINVAL` out of `pthread_mutex_lock`,
  which libc++ turns into a `std::system_error` that nothing catches. **Every
  single run ended in `SIGABRT`**, exit 134, after the work had completed.
- spirv-cross's illegal-entry-point-name set, reached from
  `CompilerMSL::compile` — `SIGSEGV`.

Fixed at the cause: the workers are joined in `~DxvkDevice` via a new
`d9mt::shutdownPsoWorkers()`, so they cannot outlive the device whose Metal
handles they compile against. The `logf` mutex is also made immortal, since
logging has to stay usable for as long as anything can call it.

Both are in `tools/patches/d3d9metal/d9mt.patch`.

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

**Read pixels, not exit codes.** `D3D9Triangle` used to print `wrote tri.tga`,
write a 1.9 MB file and return zero while drawing nothing. Every signal short of
reading the image said success. It now derives the expected colour from the same
vertex data the draw uses and makes that its exit status — which is why running
it with `D9MT_ASYNC=1` reports the original bug in one line instead of needing an
afternoon.

**A silent `return false` is worse than a crash.** The whole phase-7 blocker was
one un-logged early return in a hot path that is *designed* to fail sometimes.
When a backend can legitimately decline work, find out how it says so before
assuming it does.

**`grep -r` is `ugrep` on this machine and silently returns nothing.** Use `rg`.
And beware that `-r` means `--replace` to `rg`: `rg -rn foo` prints every match
rewritten to `n`, which looks like real output and is not.

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

**Phase 8, and async pipelines.** The game keeps d9mt's default (`D9MT_ASYNC`
unset, so on), which is the right trade for something that renders continuously:
new state costs a frame or two of missing geometry and the PSO cache in
`bin/Debug/d9mt_pso_cache.bin` warms it across runs. But the first frames of any
scene will be incomplete, so **do not diagnose a first-frame capture** — run
several frames, or set `D9MT_ASYNC=0` when a single frame has to be exact.

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
