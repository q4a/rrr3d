#!/usr/bin/env python3
"""Fetch Dear ImGui into extern/imgui, for the map editor.

Phase 12 replaces the MFC map editor with one built on Dear ImGui. That editor
is a second front end to the same Rock3dGame library the game uses -- it
differs by CreateWorld(desc, false) and RunWorldEdit() instead of RunGame() --
and its panes drive the engine's own r3d::edit::* interfaces, which already
compile on macOS and are not touched by any of this.

WHY THE DOCKING BRANCH, and not a release tag. The editor's whole layout is
dockable panes around a viewport, which is ImGuiConfigFlags_DockingEnable, and
that flag lives on the docking branch. Multi-viewport is deliberately NOT used
even though the same branch offers it: detached windows would each need their
own swapchain, and the D3D9 device here is bound to one CAMetalLayer.

WHY imgui_impl_dx9 and not a Metal backend. The device the engine owns is the
one already presenting to that layer; drawing the UI through it puts the panes
inside the engine's own frame. A second Metal pass would contend with d9mt for
the drawable and for frame pacing, which is the failure sdl_shell.cpp already
warns about for a second CAMetalLayer.

That choice carries a real risk and it is checked before anything is built on
it: imgui_impl_dx9 draws with fixed-function TnL -- D3DFVF_XYZRHW, no shaders,
D3DTSS_COLOROP -- and the engine binds a shader for every draw it makes, so
DXVK's fixed-function path has almost certainly never executed in this build.
src/D3D9ImGui exists to answer that in one command, the way src/D3D9Triangle
answered whether the backend drew at all.

Fetched into extern/ rather than committed, unlike the stb headers: these are
compiled sources with a build of their own, which is the line this repository
draws. extern/ is gitignored.

Pinned by commit AND by per-file SHA-256, so neither a moved branch nor a
rewritten commit changes what is built.

Usage:
    tools/setup-imgui.py
"""

import hashlib
import subprocess
import sys
from pathlib import Path

COMMIT = "b48d1afbe8ee8b238e2961dc363a949dd7304e23"

BASE = "https://raw.githubusercontent.com/ocornut/imgui/%s/" % COMMIT

# Core, then the two backends. Paths are relative to the repository root; the
# backends are flattened into the same directory because nothing else needs the
# distinction and the include paths stay shorter for it.
FILES = {
    "imconfig.h":
        "5755e1b8d6ab0d7811a9d7cac0f509878b51fc5ee77e09bd0f235bcb414971e7",
    "imgui.cpp":
        "304fe9be60830936a6ca1472b1a09e2af62ebff7c9a80bbcd029ff5982b26a6e",
    "imgui.h":
        "ed9911193dcbe641830ad1052e3f69e87182f876b1133484e69b082506da9a9c",
    "imgui_draw.cpp":
        "e8d6cee660dabcd50f94068834bdf87515d42fa7b52767ab5d4d13c1adf36037",
    "imgui_internal.h":
        "0e94755a322e1e4c0b09e1f86e29a08ea121c99cbd71e4785ed22173665086b2",
    "imgui_tables.cpp":
        "b5deabe5b569ab712c11b6556562a646bf88327320fc60a451071b2a36498d72",
    "imgui_widgets.cpp":
        "1e7107ad89073b4feaf65fff383e7e2a0244a2bf0063782c44640a6080ad63d0",
    "imstb_rectpack.h":
        "889b396795202d1457560a797a7242e96f6f132d4b88ca2d69be58bf05e1771f",
    "imstb_textedit.h":
        "a985f5fa0ed97353d493b497961e9eef52082edcd045cf6954b69990ec9d0741",
    "imstb_truetype.h":
        "c51a0f7e7ea760f2366bd3752635ec58e21fccfec4a832501639990ba6ce0528",
    "backends/imgui_impl_sdl3.cpp":
        "c51764f6a87e795c3844be254739c6b80f5345299972209c0a1578cf7068631d",
    "backends/imgui_impl_sdl3.h":
        "a7716305e9312d32d7ffb7806555d3ba518c6df6dbf26077d5f6a6e022c6efe5",
    "backends/imgui_impl_dx9.cpp":
        "0e7e8c165c001baa2694c0b0535406524e179440d11c7e1c7f69c80be44ade84",
    "backends/imgui_impl_dx9.h":
        "bf2d4f594af9642ff90a11aeed3ab05c27a3567d8176a30e1d10699a0d763c6c",
}

DEST = Path("extern/imgui")


def fetch(url):
    result = subprocess.run(["curl", "-sfL", url], capture_output=True)
    if result.returncode != 0:
        raise SystemExit("failed to fetch %s" % url)
    return result.stdout


def main():
    if not Path("src/Rock3dGame").is_dir():
        raise SystemExit("run this from the repository root")

    DEST.mkdir(parents=True, exist_ok=True)

    for name, expected in FILES.items():
        data = fetch(BASE + name)
        digest = hashlib.sha256(data).hexdigest()
        if digest != expected:
            raise SystemExit(
                "%s: sha256 mismatch\n  expected %s\n  got      %s"
                % (name, expected, digest))

        # Flattened: backends/x.cpp lands beside imgui.cpp.
        out = DEST / Path(name).name
        out.write_bytes(data)
        print("%s  %s (%d bytes)" % (digest[:12], out, len(data)))

    print("\nUnmodified, docking branch, pinned at %s." % COMMIT[:12])
    print("Run bin/Debug/D3D9ImGui before trusting the D3D9 backend.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
