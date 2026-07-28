# Vendored single-header libraries

## stb_image.h

`stb_image` v2.30, from <https://github.com/nothings/stb>. Dual-licensed public
domain / MIT; the licence text is at the bottom of the header itself.

Used by `src/XPlatform/source/d3dx_texture.cpp` to decode the 213 `.png` and 2
`.jpg` textures the game ships. The other 309 are `.dds`, which is parsed
directly in that file -- stb does not read DDS, and BCn data wants to reach the
GPU compressed rather than be decoded to RGBA.

**Unmodified.** Compiled with `STB_IMAGE_IMPLEMENTATION` in exactly one
translation unit, and with the decoders the game does not need switched off
(`STBI_NO_*`), which is configuration rather than a change to the source.

Vendored rather than fetched because it is one file with no build system, and
because `extern/` is gitignored -- a dependency that arrives by script has to be
buildable, and this is a header.

Chosen over macOS ImageIO deliberately: ImageIO would work and would be one less
file, but it is macOS-only, and the point of this port is a codebase that is
cross-platform with the minimum of platform-specific parts. stb_image compiles
the same everywhere.
