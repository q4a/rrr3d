#!/usr/bin/env python3
"""Run every Motor Rock .fx shader through the native macOS pipeline.

  HLSL (.fx entry point)
    -> DXSO            D3DCompile via wine
    -> SPIR-V          dxbc-spirv (DXVK 3.0's compiler)
    -> MSL             spirv-cross
    -> metallib        xcrun metal

The .fx files are effect files: they declare entry points inside
technique/pass blocks and #include shared fragments. Plain HLSL profiles
reject `technique`, so we inline the includes, extract the entry points,
strip the technique blocks, and compile each entry individually.

Caveat recorded in the output: the game drives these through
Shader::MacroBlock, which compiles each .fx many times with different
#define sets (DIFF_TEX, NORMAL, TANGENT_SPACE, ...). This pass compiles the
default permutation only -- every #if is false. Real coverage is wider.
"""

import os
import pathlib
import re
import subprocess
import sys


def _env(name, default=None):
    v = os.environ.get(name, default)
    if not v:
        sys.exit(f"error: set {name}")
    return pathlib.Path(v)


SHADERS = _env("RRR3D_SHADER_DIR")
DXBC = _env("RRR3D_DXBC_COMPILER")
FXC = _env("RRR3D_FXCOMPILE")
WINE = os.environ.get("RRR3D_WINE", "wine")
OUT = pathlib.Path(os.environ.get("RRR3D_OUT", "fxout"))

INCLUDE = re.compile(r'^\s*#include\s+"([^"]+)"', re.M)
TECHNIQUE = re.compile(r'technique\s+\w+\s*\{.*?\n\}', re.S)
COMPILE = re.compile(r'compile\s+(vs|ps)_(\d)_(\d)\s+(\w+)\s*\(')


def inline_includes(path, seen=None):
    """Recursively splice #include "x.fx" -- D3DCompile gets a null handler."""
    if seen is None:
        seen = set()
    text = path.read_text(encoding="utf-8", errors="replace")

    def sub(m):
        name = m.group(1)
        if name in seen:
            return ""
        seen.add(name)
        target = SHADERS / name
        return inline_includes(target, seen) if target.exists() else ""

    return INCLUDE.sub(sub, text)


def run(cmd, **kw):
    return subprocess.run(cmd, capture_output=True, text=True, **kw)


def main():
    OUT.mkdir(exist_ok=True)
    rows, totals = [], {"entries": 0, "dxso": 0, "spv": 0, "msl": 0, "lib": 0}

    for fx in sorted(SHADERS.glob("*.fx")):
        source = inline_includes(fx)
        entries = [(f"{k}_{a}_{b}", name) for k, a, b, name in COMPILE.findall(source)]
        stripped = TECHNIQUE.sub("", source)

        if not entries:
            rows.append((fx.name, "-", "(include-only, no techniques)", ""))
            continue

        for profile, entry in entries:
            totals["entries"] += 1
            base = f"{fx.stem}_{entry}"
            hlsl = OUT / f"{base}.hlsl"
            hlsl.write_text(stripped, encoding="utf-8")

            stage, detail = "DXSO", ""
            r = run([str(WINE), str(FXC), str(hlsl), entry, profile, str(OUT / f"{base}.bin")],
                    env={**os.environ, "WINEDEBUG": "-all"})
            if r.returncode == 0:
                totals["dxso"] += 1
                stage = "SPIR-V"
                r = run([str(DXBC), "--spv", str(OUT / f"{base}.spv"), str(OUT / f"{base}.bin")])
                if r.returncode == 0:
                    totals["spv"] += 1
                    stage = "MSL"
                    r = run(["spirv-cross", "--msl", "--msl-version", "30000",
                             "--msl-argument-buffers", "--msl-argument-buffer-tier", "2",
                             str(OUT / f"{base}.spv"), "--output", str(OUT / f"{base}.metal")])
                    if r.returncode == 0:
                        totals["msl"] += 1
                        stage = "metallib"
                        r = run(["xcrun", "-sdk", "macosx", "metal", "-o",
                                 str(OUT / f"{base}.metallib"), str(OUT / f"{base}.metal")])
                        if r.returncode == 0:
                            totals["lib"] += 1
                            stage = "OK"

            if stage != "OK":
                err = (r.stderr or r.stdout or "").strip().split("\n")
                detail = next((l for l in err if "error" in l.lower()), err[0] if err else "")[:70]
            rows.append((fx.name, f"{entry} ({profile})", stage, detail))

    print(f"{'shader':<20} {'entry':<26} {'reached':<10} note")
    print("-" * 96)
    for f, e, s, d in rows:
        print(f"{f:<20} {e:<26} {s:<10} {d}")

    print(f"\nentry points: {totals['entries']}")
    for k, label in [("dxso", "-> DXSO"), ("spv", "-> SPIR-V"), ("msl", "-> MSL"), ("lib", "-> metallib")]:
        print(f"  {label:<14} {totals[k]:>3}/{totals['entries']}")
    print("\nNote: default macro permutation only (every #if false). The game")
    print("compiles each .fx many times via Shader::MacroBlock.")
    return 0 if totals["lib"] == totals["entries"] else 1


if __name__ == "__main__":
    sys.exit(main())
