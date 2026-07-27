# Shader pipeline validation

Two checks live here, answering different questions.

## 1. Can the shaders be compiled natively? — `vkd3d_hlsl_check.py`

The one the runtime depends on. The game compiles its `.fx` files *at runtime*,
through `D3DXCreateEffect`, with a different `#define` set each time — so it
needs a real HLSL compiler in the process, emitting the shader model 3 bytecode
a D3D9 device accepts.

    tools/setup-vkd3d-macos.sh          # once
    python3 tools/shader-pipeline/vkd3d_hlsl_check.py

Result: **48/48 entry points compile to `vs_3_0`/`ps_3_0` with vkd3d-shader**,
natively, with no Wine and no Microsoft `d3dcompiler`.

That matters because check 2 below, and everything it proved, runs
`D3DCompile` under Wine. That is a fine investigation tool and can never ship:
it is neither native nor ours.

DXC and glslang were the alternatives considered. Both target DXBC or SPIR-V;
neither emits shader model 3, which is the only thing D3D9 takes.

## 2. Do they survive the whole path to Metal? — `batch_shaders.py`

Checks that this game's shaders survive the path a native macOS graphics
backend would put them through:

```
.fx entry point
  --> DXSO      D3D9 SM3 bytecode, via D3DCompile under wine
  --> SPIR-V    dxbc-spirv, the shader compiler DXVK 3.0 replaced its
                legacy translator with
  --> MSL       spirv-cross, Metal argument buffers tier 2
  --> metallib  xcrun metal
```

## Why this exists

The plan for macOS is to keep DXVK's D3D9 front-end and put a Metal backend
behind `DxvkContext`, rather than reimplementing D3D9. DXVK 3.0 deleted
`src/dxso/` — its entire legacy shader translator — and replaced it with
`dxbc-spirv`. That made the shader path the single thing most likely to
invalidate the approach, so it is checked first and kept as a regression test.

Result at time of writing: **48/48 entry points reach metallib**, covering
every `.fx` the game ships, including `water`, `hdr`, `bumpMap`,
`reflBumpMapp`, `ShadowMap`, `sunShaft` and `grassFiled`.

## What it does not prove

- **Compiles, not renders.** Nothing here touches a GPU. It shows the
  toolchain accepts the shaders, not that the output is correct.
- **Default macro permutation only.** Every `#if` is false. The game drives
  these through `Shader::MacroBlock`, compiling each `.fx` many times with
  different `DIFF_TEX` / `NORMAL` / `TANGENT_SPACE` sets, so real coverage is
  a multiple of 48. This is a floor.
- Nothing about the ~95 `DxvkContext` methods a backend still has to implement.

## Requirements

    brew install meson ninja spirv-cross mingw-w64
    # wine, with a prefix that has d3dcompiler available

    git clone --recurse-submodules https://github.com/doitsujin/dxbc-spirv
    meson setup dxbc-spirv/build dxbc-spirv --buildtype=release \
        -Denable_tools=true -Denable_sm3=true
    ninja -C dxbc-spirv/build

## Usage

    i686-w64-mingw32-gcc -o fxcompile.exe fxcompile.c -ld3dcompiler

    RRR3D_SHADER_DIR=/path/to/Motor\ Rock/Data/Shaders \
    RRR3D_DXBC_COMPILER=/path/to/dxbc-spirv/build/tools/dxbc_compiler \
    RRR3D_FXCOMPILE=./fxcompile.exe \
        python3 batch_shaders.py

Exits non-zero if any entry point fails to reach metallib.

## Notes

`.fx` files are effect files: entry points are declared inside
`technique`/`pass` blocks, and plain HLSL profiles reject `technique`. The
script inlines `#include`s (D3DCompile is given a null include handler),
extracts the entry points, strips the technique blocks, and compiles each
entry separately.
