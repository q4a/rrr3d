# D3D9Metal — Direct3D 9 on Metal

Two vendored trees plus the small set of changes that make them build natively.

| Part | Origin | Lines |
|---|---|---|
| `dxvk/` | [DXVK](https://github.com/doitsujin/dxvk) v2.7.1 — the D3D9 front-end | ~22,400 (d3d9) |
| `d3d9fe/` | [d9mt](https://github.com/neo773/d9mt) — the Metal `DxvkDevice`/`DxvkContext` | ~17,500 |
| `spirv-cross/` | SPIR-V → MSL | — |
| `generated/` | DXVK's compiled-from-GLSL format-conversion shaders | — |
| `winemetal/` | the ABI contract; implemented in `src/MetalBridge` | — |

The pieces fit together as: **D3D9 API → DXVK front-end → `DxvkContext` →
d9mt's Metal backend → winemetal ABI → `src/MetalBridge` → Metal.**

## No Wine

d9mt targets Wine and reaches Metal through `winemetal`, an ABI whose job is
crossing wow64. There is no boundary here, so `src/MetalBridge` implements that
ABI directly against Metal and the boundary disappears. Nothing in this
directory depends on Wine — `d3d9fe/` never did; its only Wine references were
comments.

## Changes to the vendored trees

Kept to the minimum, and every one is a 32-bit or Wine artefact rather than a
design change:

- **Vulkan handle casts** (4 sites). Non-dispatchable handles are `uint64` on
  i686 and pointers on arm64. d9mt smuggles Metal handles through them, which
  is fine either way, but the cast has to change shape.
- **Relative includes rebased.** d9mt reached into `../../vendor/...` and DXVK
  headers were patched to include `d9mt_fetrace.h` by path. Both assumed
  d9mt's layout.
- **`Win32WSI`** is defined in `d9mt_wsi_bootstrap.cpp` instead of coming from
  DXVK's Win32 file. See that file for why.
- **`D9MT_API`** is `__declspec` on Windows and empty here — no DLL boundary.

Everything else compiles unmodified. The Win32 surface both trees expect —
`VirtualAlloc`, `LoadLibraryA`, `QueryPerformanceCounter`, the D3D11 and D3D12
interop headers — is in `src/XPlatform`.

## Still to do

- `Direct3DCreate9` is not yet wired to `src/XPlatform/source/d3d9_stub.cpp`.
- The WSI driver needs a real window; `installWsiDriver()` must run before
  `wsi::init()`.
- d9mt runs GTA IV but has never rendered 3D for *this* game, and whether that
  was a backend bug or a Wine artefact is unestablished. A native build removes
  the boundary, so getting here also answers it. `~/src/d9mt/traces/` holds
  captured frames of this game to diff against.
