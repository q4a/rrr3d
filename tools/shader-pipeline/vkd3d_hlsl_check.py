#!/usr/bin/env python3
"""Compile every .fx entry point to Direct3D 9 shader model 3 with vkd3d-shader.

This answers one question: can the shaders this game ships be compiled
natively, in-process, without Wine and without Microsoft's d3dcompiler?

The rest of this directory answers a different and larger question -- whether
the shaders survive the whole path down to metallib -- but it does so with
D3DCompile running under Wine, which can validate and can never ship. The
runtime effects framework needs a compiler it can call directly, and this
checks the one chosen for it.

Entry points are found the same way the effects framework finds them: every
`VertexShader = compile vs_3_0 Foo()` / `PixelShader = compile ps_3_0 Bar()`
inside a technique's pass.

Caveat, the same one batch_shaders.py records: this compiles the default macro
permutation only -- every #if is false. The game drives these through
Shader::MacroBlock with different DIFF_TEX / NORMAL / TANGENT_SPACE sets, so
real coverage is a multiple of what is reported here. This is a floor.

Requires extern/vkd3d, built by tools/setup-vkd3d-macos.sh.

  usage: vkd3d_hlsl_check.py [shader_dir]
"""

import pathlib
import re
import subprocess
import sys

REPO_ROOT = pathlib.Path(__file__).resolve().parents[2]
VKD3D = REPO_ROOT / "extern" / "vkd3d"
HERE = pathlib.Path(__file__).resolve().parent

DEFAULT_SHADER_DIR = REPO_ROOT / "bin" / "Debug" / "Data" / "Shaders"

# The .fx files are CP1251 -- they carry Russian comments, like the rest of the
# original source did before the tree was transcoded. The game data was not.
FX_ENCODING = "cp1251"

COMPILE_RE = re.compile(
    r"(?:VertexShader|PixelShader)\s*=\s*compile\s+(\w+)\s+(\w+)\s*\(")


def build_checker():
    """Compile the C harness against the vkd3d-shader built into extern/."""
    source = HERE / "vkd3d_hlsl_check.c"
    binary = HERE / "vkd3d_hlsl_check"

    lib = VKD3D / "build" / ".libs" / "libvkd3d-shader.a"
    if not lib.exists():
        sys.exit(f"error: {lib} not found -- run tools/setup-vkd3d-macos.sh first")

    subprocess.run(
        ["clang", "-O2", "-o", str(binary), str(source),
         "-I", str(VKD3D / "include"), str(lib)],
        check=True)
    return binary


def main():
    shader_dir = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else DEFAULT_SHADER_DIR
    if not shader_dir.is_dir():
        sys.exit(f"error: {shader_dir} is not a directory")

    checker = build_checker()

    passed = 0
    failures = []

    for fx in sorted(shader_dir.glob("*.fx")):
        text = fx.read_bytes().decode(FX_ENCODING)
        for profile, entry in COMPILE_RE.findall(text):
            result = subprocess.run(
                [str(checker), str(shader_dir), fx.name, profile, entry],
                capture_output=True, text=True)
            if result.returncode == 0:
                passed += 1
            else:
                failures.append(result.stdout.strip() or result.stderr.strip())

    total = passed + len(failures)
    print(f"{passed}/{total} entry points compiled to D3D9 shader model 3")

    for failure in failures:
        print()
        print(failure)

    return 1 if failures else 0


if __name__ == "__main__":
    sys.exit(main())
