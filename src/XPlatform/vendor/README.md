# Vendored single-header libraries

Fetched by `tools/vendor-stb.py`, which pins both the upstream commit and each
file's SHA-256. Re-running it is how these are updated; they are not edited.

## stb_image.h

`stb_image` v2.30, from <https://github.com/nothings/stb>. Dual-licensed public
domain / MIT; the licence text is at the bottom of the header itself.

Used by `src/XPlatform/source/d3dx_texture.cpp` to decode the 213 `.png` and 2
`.jpg` textures the game ships. The other 309 are `.dds`, which is parsed
directly in that file — stb does not read DDS, and BCn data wants to reach the
GPU compressed rather than be decoded to RGBA.

**Unmodified.** Compiled with `STB_IMAGE_IMPLEMENTATION` in exactly one
translation unit, and with the decoders the game does not need switched off
(`STBI_NO_*`), which is configuration rather than a change to the source.

## stb_truetype.h

`stb_truetype` v1.26, same repository and licence.

Used by `src/XPlatform/source/d3dx_font.cpp` for `ID3DXFont`. That is not a
debug-only concern here: `graph::TextFont` is a first-class engine resource and
every piece of UI text in the game renders through it, so compiling it out would
remove all text from the game.

**Unmodified**, compiled with `STB_TRUETYPE_IMPLEMENTATION` in one translation
unit.

## Why committed rather than fetched into `extern/`

`extern/` is gitignored on the principle that a dependency arriving by script
has to be *built* by that script. These are headers with no build system, so
there is nothing to build; the script is a provenance record and the files live
in the tree. Same reasoning as `src/TinyXml`.

## Why not ImageIO and CoreText

Both would work, and both would be less code than this. Both are also
macOS-only, and the point of this port is a codebase that is cross-platform with
the minimum of platform-specific parts. These two headers compile the same
everywhere, which is worth more than the lines they cost.

## stb_dxt.h

`stb_dxt` v1.12, from the same pinned commit. Same dual licence.

Used by `src/XPlatform/source/d3dx_texture.cpp` to implement
`D3DXFilterTexture`. The engine calls that on block-compressed textures whose
level 0 it has just filled from file data (`VideoResource.cpp:732`), so the mip
chain has to be produced in the compressed domain: decode each level, box
filter in RGBA, re-encode. This is the re-encode half; the decode is ours,
because stb does not provide one.

201 of the game's 202 mip-generating textures are DXT, so this is essentially
every world, track and car surface. Without it their lower levels are
whatever `CreateTexture` left in system memory — and an all-zero DXT1 block
decodes to solid black, not to something merely blurry.

**Unmodified.** Compiled with `STB_DXT_IMPLEMENTATION` in exactly one
translation unit.
