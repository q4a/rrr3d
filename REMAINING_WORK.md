# Remaining work — macOS port, attempt 2

Branch `macos-port-attempt-2`, based at `ec50208` (2021-06-14). 63 commits.

**The game runs.** It reaches its main menu, loads a track and renders a race
at 60fps, with sound. `bin/Debug/race2.tga` is a frame from three thousand
frames in. It is not yet playable: with audio enabled a race dies within a
second, and the menu does not respond to the keyboard.

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
| 5 — SDL3 shell | done: keystrokes reach the game, verified by trace |
| 6 — MetalBridge | done, all six test modes pass |
| 7 — D3D9 on Metal | done: the triangle draws, both paths verified by pixel |
| 8 — D3DX runtime, first pixels | done: menu and race both render |
| 9 — `NxWheelShape` | done: implemented and covered; **cars do not drive yet** |
| 10 — Windows cutover | deferred by decision, not dropped |
| 11 — Audio, gamepad, video | audio over FAudio (**crashes a race**), gamepad done, video stubbed |
| 12 — MapEditor on Dear ImGui | not started |

Zero undefined symbols. Audio is a real FAudio backend
(`xaudio2_faudio.cpp`); video is still a stub (`video_stub.cpp`) reporting
`STATE_NO_GRAPH`, which the game tolerates, so cutscenes are skipped rather
than broken.

### Running it

    cd bin/Debug
    ./RRR3d                                   # the menu
    RRR3D_AUTORACE=1 ./RRR3d                  # straight into a race

    RRR3D_DUMP_FRAME=<n> ./RRR3d              # write frame n and carry on
    RRR3D_DUMP_PATH=<file>                    # default frame.tga
    RRR3D_WHEEL_TRACE=1 ./RRR3d               # suspension rays and what they hit
    RRR3D_INPUT_TRACE=1 ./RRR3d               # keystrokes as the game receives them
    RRR3D_AUDIO_NO_CALLBACKS=1 ./RRR3d        # audio plays, game callbacks withheld

A `CAMetalLayer`'s contents never appear in `screencapture` — the screenshot
comes back as the window frame with a hole where the game is — so the dumper is
the only way to see anything. It lives in `Engine::Present`, **not**
`D3D9RenderDriver::Present`: the engine calls the device directly and skips
that wrapper, which took one dump that never fired to discover.

`RRR3D_AUTORACE` is the authors' own debug path, not a new one — the `#if
DEBUG_PX` block in `GameMode::StartGame`, commented out in the shipped source,
is the same sequence.

### Test suites, all green

    bin/Debug/Tests            156 checks   XPlatform, D3DX math, string and RNG properties
    bin/Debug/PhysX28Tests       0 failures constants, conventions, no simulation
    bin/Debug/PhysX28Harness   512 checks   the shim against 2.8's specification
    bin/Debug/BridgeTriangle   6 modes      run from bin/Debug; takes a mode argument
    bin/Debug/D3D9Triangle     2 modes      no argument = swapchain, `rtt` = own target

`BridgeTriangle` modes: `vertexid stagein argbuf nocopy speccnst blend`. Each
adds one layer over the last; all render offscreen with pixel readback.

`D3D9Triangle` checks the pixels itself and exits non-zero if the triangle is
missing; it also writes `tri.tga` / `tri_rtt.tga` to look at.

---

## What is actually left

### The blocker: FAudio's resampler writes out of bounds

Audio works — the game plays sound — but with audio enabled a race dies within
about a second of starting. This is the one thing standing between the port and
being playable.

**Address Sanitizer names the write** (`build/macos-arm64-asan` is configured and
working):

    ERROR: AddressSanitizer: BUS on unknown address ... caused by a WRITE
      #0 FAudio_INTERNAL_ResampleMono_NEON
      #1 FAudio_INTERNAL_MixCallback
      #2 SDL_GetAudioStreamDataAdjustGain      (SDL audio thread)

"unknown address" rather than a heap overflow because FAudio is an
uninstrumented Homebrew dylib — the sanitizer cannot see its buffers.

**Measured, eight short runs per condition:**

| condition | died early |
|---|---|
| audio off entirely (`RRR3D_AUDIO_OFF=1`) | **0/8** |
| audio on, callbacks withheld (`RRR3D_AUDIO_NO_CALLBACKS=1`) | 8/8 |
| audio on, `SetFrequencyRatio` clamped to the voice's max | 8/8 |
| audio on, redundant identical `SetOutputVoices` skipped | 8/8 |
| audio on, unmodified | 8/8 |

So it is **not** the callbacks, the sends, the frequency ratio, or voice
destruction. The only variable that changes the outcome is whether a FAudio
engine exists and mixes at all. Everything the shim hands FAudio has been
checked and is sane: every voice is PCM 44100/16-bit with correct block align,
flags 0, maxRatio 2.0; every buffer has `PlayBegin=0, PlayLength=0` with a
frame count consistent with `AudioBytes`; every send resolves through
`FaudioOf` to a real `FAudioVoice*`, never NULL.

**Next step, and it is a measurement rather than a patch.** In FAudio's source,
the write target is the *shared* `audio->resampled_audio`, grown by

    static void resize_resampled_audio_buffer(FAudio *audio, uint32_t samples)
    {
        if (samples > audio->resampleSamples) {
            audio->resampleSamples = samples;
            audio->resampled_audio = audio->pRealloc(...);   /* never NULL-checked */
        }
    }

`FAudioCreateWithCustomAllocatorEXT` lets the shim supply malloc/realloc/free.
Installing one and logging every size and result names the fault exactly — an
absurd size or a NULL return — with nothing left to theorise about. Roughly ten
lines in `XAudio2Create`, and it cannot be confounded by anything else.

**Five fixes were attempted before that and all were wrong.** They are listed
above so nobody repeats them. Each came from a plausible story about the crash;
plausible stories are cheap. Only the measurements were worth anything, and the
lesson is the one already in this file — measure first.

### Everything else

**Handling has not been compared against 2.8.** The wheels work —
`RRR3D_WHEEL_TRACE=1` shows every sampled suspension ray hitting, with wheel
origins spread around the whole circuit, so the AI cars race properly. What has
not been checked is whether a car *drives* the way 2.8 drove it: acceleration,
cornering, that it does not creep when parked, and that the suspension does not
ring at the low damping ratios the shipped cars use.

That needs a driver. The player's car has none under `RRR3D_AUTORACE` — the
camera follows it, so a frame from a race shows a stationary car while the AI
races off. Either hold the throttle from the autorace hook, or verify input and
drive it.

A caution earned the hard way: **do not diagnose the physics from one frame.**
Reading a single frame is what produced a confident and wrong conclusion that
the suspension raycast was broken. The trace samples periodically for exactly
this reason — the first frames are all spawn transient.

**The menu ignores the keyboard, though the keyboard works.**
`RRR3D_INPUT_TRACE=1` shows real keystrokes arriving at
`GameMode::OnHandleInput` correctly mapped — Down to `gaBreak`, Up to
`gaAccel`, Return to `gaAction` — so SDL, the scancode table, the view and the
binding table are all sound. But a frame taken after Down, Return, Down, Return
is the unchanged main menu with nothing highlighted. The break is between the
game receiving a `GameAction` and the GUI acting on it.

**`D3DXFilterTexture` is unimplemented**, reported once per race. It generates
the mip chain for a texture the engine rendered into, so the lower levels are
undefined rather than absent — aliasing at distance, not a black surface.
Either implement a box filter down the chain or record it as accepted.

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

**`_MyBase::f(x)` is not a virtual call.** Fixing a two-phase-lookup error by
qualifying the name compiles and silently turns virtual dispatch into a static
call to the empty base body. It cost the whole component tree: no child ever
learned its owner, so every cross-reference saved as an absolute component path
failed to resolve on load, and every `Object*` container quietly stopped
reference counting. `this->f(x)` is the fix — it makes the name dependent,
which is all the lookup needed, and leaves dispatch alone. When sweeping for
this, only names that are actually `virtual` matter; the tree has dozens of
deliberate `_MyBase::Save(...)` chaining calls that are exactly right.

**When splitting one statement into two, check for a single-statement `if`.**
Doing this turned conditional `SendEvent` calls into unconditional ones three
times.

**Three Windows assumptions are baked into this codebase's data, not its
code**, and each failed silently rather than loudly:

- Paths are written with backslashes, in source literals and in `db.xml` alike.
  A backslash is an ordinary filename character on POSIX, so the whole path
  becomes one name. Translated once in `GetAppFilePath`, which every asset load
  passes through.
- The language files are UTF-16LE, and the loader cast the buffer to `wchar_t*`
  — right only where `wchar_t` is 16 bits. The menu drew `svSingleGame` instead
  of `Single Player` and nothing errored.
- `RAND_MAX` is 32767 on MSVC and 2147483647 here, so an expression that fits an
  `int` there overflows here. `RandomRange` did it twice, in the divisor and in
  the numerator, and the second one produced indices anywhere at all — which
  crashed in the AI, several frames later, with nothing pointing back.

**A crash is a gift; behaving differently is not.** Of the eight bugs found
between the first build and a rendering race, six were silent: the qualified
virtual call, the path separators, the UTF-16 cast, `createActor` ignoring its
shape list, async PSO compilation skipping draws, and `RandomRange`. Only the
mesh use-after-free and the audio stub's missing virtual destructor announced
themselves.

---

## Phase 4 — what the shim does and does not do

Implemented and tested against 2.8's specification, not against a feel target:
scene, actors, box/sphere/capsule/plane/mesh shapes, materials, collision
filtering, contact reports and the stream iterator, contact modification,
raycasts, triangle-mesh cooking, per-shape skin width, centre-of-mass offset.

Phase 9 added `NxWheelShape` — see `src/PhysX28/source/Px28Wheel.cpp`, which
carries the specification it is written against. One `Unimplemented()` call
remains: a `setTiming` guard that fires only if `maxTimestep` drops below 1/60.

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
- **`createActor` builds the shapes its descriptor lists**, in descriptor
  order. `UnpackActorShapeListIncludeChildren` walks `getShapes()` positionally
  against its own list, so shape *i* must come from descriptor *i* — any other
  order wires every shape to the wrong engine object without failing anything.
- **Releasing a mesh does not destroy it** while shapes still reference it. 2.8
  reference counts them, which is why `NxTriangleMesh` has a
  `getReferenceCount()`. `px::Actor::ReloadNxShape` builds the replacement shape
  *before* releasing the one it replaces, so eager destruction is a
  use-after-free on every track load.
- **A wheel's tire curve is a friction ceiling, not a force**, under
  `NX_WF_CLAMPED_FRICTION` — which every car sets. Reading it as a force is the
  difference between a car that drives and one that stands on its back wheels.
- **The wheel suspension must be implicitly integrated.** The shipped cars run
  damping ratios from 0.71 down to about 0.06; explicit integration lets the
  undamped ones ring, which swings tire load and reads as a grip problem.

### Verification style

Every scenario is checked with a deliberate mutation to prove it fails, and the
mutation is recorded rather than just performed. The ones that earned their
place: transposing both transform directions (a round trip alone cannot see
it), half-extents as full extents, dropping the material slot-0 reservation (12
failures), an asymmetric group matrix, reporting impulse as force (0.8175
instead of 49.05 — exactly 1/60), removing `createActor`'s shape loop (3
failures), and restoring `RandomRange`'s int multiply (3 failures, escaping
with −1).

Two mutations do something better than fail. Making critical sections
non-recursive **hangs** `src/Tests`, which is what the engine would do. And
restoring eager mesh destruction **segfaults** the harness, in the same place
the game did — the scenario reproduces the crash rather than merely detecting
its absence.

---

## Notes on the remaining phases

**Phase 8, and async pipelines.** The game keeps d9mt's default (`D9MT_ASYNC`
unset, so on), which is the right trade for something that renders continuously:
new state costs a frame or two of missing geometry and the PSO cache in
`bin/Debug/d9mt_pso_cache.bin` warms it across runs. But the first frames of any
scene will be incomplete, so **do not diagnose a first-frame capture** — run
several frames, or set `D9MT_ASYNC=0` when a single frame has to be exact.

**Phase 9's wheel, and what is chosen rather than specified.** Two things the
SDK does not define are marked in `Px28Wheel.cpp` where they occur: how
`inverseWheelMass` becomes a rotational inertia (a disc, `I = m·r²/2`, which is
the SDK's own word for the wheel), and the exact implicit spring integration.
Both are derived from what the API states, and neither reads a game constant.
If handling ever needs investigating, those two are the honest places to look
first — and the answer is still not to tune them against how the car feels.

**Phase 11's seams are already in place and are not equivalent.**
`xaudio2_stub.cpp` is a silent *implementation*, because three of its answers
are read back and acted on: `GetState` must report an empty queue or the
streaming code stops feeding, `GetVolume` must return what `SetVolume` was
given or fades freeze instead of silencing, and `X3DAudioCalculate` must fill
the matrix the caller allocates and passes straight on. `video_stub.cpp` is a
true stub, because `STATE_NO_GRAPH` is a state the game already knows how to be
in — `World::IsVideoPlaying()` tests for exactly it.

**Phase 11's other half.** The plan is wrong about FAudio: it ships no
`xaudio2.h`; its
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
