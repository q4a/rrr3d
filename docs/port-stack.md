# The macOS port stack

What each Windows-only dependency was replaced with, and why. Measured against
the merge base `ec50208` ("add clang-format"): 428 files changed, +151,690 /
-6,239.

A note on a common guess: **wined3d is not used.** D3D9 is served by DXVK. Wine
code does appear, but only as two small pieces -- its `d3dx9math.c` and its
`vkd3d-shader` HLSL compiler.

## Summary

| Subsystem | Was (merge base) | Now | Tech |
|---|---|---|---|
| Shell / window / input | `_tWinMain`, `HWND`, `WndProc` | `src/RRR3d/sdl_shell.cpp` | **SDL3** |
| Win32 API surface | MSVC + Platform SDK | `src/XPlatform` | hand-written headers |
| D3D9 | `d3d9.lib` | `src/D3D9Metal` + `src/MetalBridge` | **DXVK v2.7.1** -> **d9mt** -> **Metal** |
| Shader translation | D3D9 bytecode -> driver | compile chain | DXVK -> SPIR-V -> **SPIRV-Cross** -> MSL |
| D3DX utility library | `d3dx9.lib` | `src/XPlatform/source/d3dx_*.cpp` | **libvkd3d-shader**, **stb**, Wine `d3dx9math.c` |
| Audio | XAudio2 | `src/XPlatform/source/xaudio2_faudio.cpp` | **FAudio 26.06** |
| Physics | PhysX 2.8 | `src/PhysX28` | **Bullet** |
| Video | DirectShow | `src/Rock3dGame/source/video/` | **VideoToolbox** + **AudioToolbox** + own demuxer |
| Map editor | MFC | `src/MapEditor/editor_main.cpp` | **Dear ImGui** (docking branch) |

## Per subsystem

### Shell, window, input -- SDL3

The one genuinely new dependency at the top of the stack. `sdl_shell.cpp`
replaces `_tWinMain`; SDL3 supplies the window, the event pump, mouse and
keyboard, and the gamepad behind XPlatform's `xinput.h` -- so
`ControlManager.cpp` still calls `XInputGetState` and does not know anything
changed. SDL was entirely absent at the merge base; the only `sdl` matches there
were an unrelated `sdLeft` scroll-direction enum in `GUI.h`.

`RRR3d.cpp` is kept and still selected on MSVC.

### Win32 surface -- `src/XPlatform`, no third party

`windows.h`, `tchar.h`, `winuser.h`, `wingdi.h`, `mmsystem.h`, `crtdbg.h`,
`dshow.h`, `xinput.h`, `x3daudio.h`, `xaudio2.h` -- each written to the exact
depth the game's own headers reach, and no deeper. Guarded by
`#ifdef _WIN32 #error` so it can never be compiled on Windows by accident.

### Graphics -- DXVK's D3D9 front-end, not wined3d

`src/D3D9Metal` compiles DXVK v2.7.1's D3D9 implementation out of `extern/`,
including its fixed-function TnL (which is what `imgui_impl_dx9` needs, and what
the editor rides on). Underneath it, **d9mt** (`neo773/d9mt`) swaps DXVK's
Vulkan backend for Metal.

`src/MetalBridge` is ours: ObjC++ command encoding, pipeline state and present,
plus `d9mtmetal.m`.

Shaders take the full chain: D3D9 bytecode -> DXVK's shader compiler -> SPIR-V
-> **SPIRV-Cross** -> MSL -> Metal.

### D3DX -- three different sources, because it is three different problems

DXVK implements D3D9 but *not* D3DX, which is a separate utility library:

- **Math** -- Wine's `d3dx9math.c`, vendored by `tools/vendor-wine-d3dx9math.py`
  into `src/MathLib`. Excluded from the MSVC build, which links the real
  `d3dx9.lib` instead.
- **Effects and HLSL** -- `d3dx_effect.cpp` calls **libvkd3d-shader** (Wine's
  `vkd3d_shader_compile` / `vkd3d_shader_preprocess`) as the compiler behind
  `D3DXCreateEffect`, wrapped so nothing downstream needs to know vkd3d exists.
- **Textures and fonts** -- `d3dx_texture.cpp` and `d3dx_font.cpp` over **stb**:
  `stb_image.h`, `stb_dxt.h` for the BC1/BC3/BC5 re-encode inside
  `D3DXFilterTexture`, and `stb_truetype.h`.

### Audio -- FAudio 26.06

`xaudio2_faudio.cpp` implements the hand-written `xaudio2.h` (XAudio2 2.7 shape)
on top of FAudio.

The version *is* the fix. 26.07 and 26.08 corrupt memory on the engine-rev
frequency sweep -- the resampler's tap loops are unbounded with respect to the
destination buffer, and `toResample - tap_samples` underflows. So
`tools/setup-faudio-macos.sh` pins the tag, and `src/XPlatform/CMakeLists.txt`
warns loudly if the checkout drifts off it. See
`tools/patches/faudio/26.08-not-applied.patch` for the diagnosis, which is
recorded but deliberately not applied.

### Physics -- Bullet behind a PhysX 2.8 shim

`src/PhysX28` reimplements the slice of the PhysX 2.8 API the game actually
calls. The backend was chosen by the trial in `docs/physics-backend-trial.md`.

Bullet is linked `PRIVATE` and named in exactly one CMakeLists, so no Bullet
header, type or macro can reach a consumer.

### Video -- Apple frameworks, plus code that had to be written

No library ships an AVI demuxer or the display-order logic, so this subsystem is
the most hand-written:

- own RIFF/AVI demuxer (`avi_reader.cpp`);
- **VideoToolbox** for H.264, via `CoreMedia` and `CoreVideo`, converting Annex B
  to AVCC;
- **AudioToolbox** for the MP3 track;
- own H.264 picture-order-count parser (`h264_poc.cpp`). This one is not
  optional: VideoToolbox does not derive presentation order from the bitstream,
  and given an invalid PTS it hands the same invalid PTS back -- so B-frame
  ordering has to come from POC parsed by hand.

Frames are presented as a D3D9 texture inside the engine's own frame rather than
through a sibling layer, which avoids contending with d9mt for the
`CAMetalLayer` drawable.

### Map editor -- Dear ImGui, docking branch

Pinned by commit in `tools/setup-imgui.py`. Uses `imgui_impl_sdl3` and
`imgui_impl_dx9`, drawing through the same D3D9 device the engine already owns,
via a new `IOverlay` seam (`IWorld::SetOverlay` -> `GraphManager::DrawOverlay`)
that the engine previously had no equivalent of -- MFC never needed one, because
it drew its panes with GDI into sibling `HWND`s.

This is not a port of the MFC code. The editor's logic always lived in the
engine's `r3d::edit::*` interfaces, which compiled on macOS untouched; immediate
mode deletes the `HTREEITEM` tree-item/engine-object bookkeeping outright, which
is where the line count went down rather than across.

### Already portable, left alone

ogg, vorbis and Boost 1.69 (Asio, bind). Boost needed pinning, not replacing --
the MSVC auto-link pragma wanted a 1.69 binary that nothing provides, so
`BOOST_ALL_NO_LIB` turns it off and the header-only guarantee carries it.
TinyXml was vendored into `src/TinyXml`.

## The pattern

Almost none of this is engine rewriting. It is substitution at the library
boundary, which is why roughly 130k lines of game code survive with `#ifdef`s
that keep the Windows path intact -- and, as of the CI work, keep it compiling
and linking in both MSVC configurations.

Every dependency is pinned by tag, commit, or SHA-256 through a `tools/setup-*`
or `tools/vendor-*` script. The two pinned *hard* -- FAudio 26.06 and DXVK
v2.7.1 -- are pinned because drift there was measured to break things, not out of
caution.
