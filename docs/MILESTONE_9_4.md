# Milestone 9.4: cube reflections, tangent-space materials and FxTrail

## Result

M9.4 closes the three explicit renderer gaps left by M9.3 without adding a
parallel game implementation:

- `GraphManager::InitTrueRefl` / `RenderCubeMap` now have a bgfx/Metal
  six-face render target;
- `BumpMapShader` receives the original secondary normal-map sampler and
  tangent basis calculated from `.r3d` geometry;
- `FxTrailManager::DrawPath` is represented by connected transient strip
  geometry driven by the original `ctEffects/trail` record and actual wheel
  position history.

No Parallels/Windows run is part of this milestone.

## True environment reflection

The renderer abstraction now owns a `CubeRenderTarget`: one sampleable cube
texture plus six face framebuffers. The Metal backend creates a 512×512 BGRA8
cube and a depth attachment for every face. Depth attachments explicitly use
`BGFX_RESOLVE_NONE`, as required by bgfx framebuffer validation.

The per-frame order mirrors the source graph:

1. directional shadow;
2. environment +X, -X, +Y, -Y, +Z and -Z;
3. conditional planar reflection;
4. main HDR scene and the existing M9.3 post chain.

The six camera direction/up pairs and 90-degree projection are copied from
`GraphManager::RenderCubeMap`. Their center follows the player vehicle with
the original one-unit Z offset. While the cube is populated, reflective
objects use the original static sky DDS, avoiding recursive feedback. The
completed cube is then bound to `glRefl` scene and planar-reflection draws.

Sky rendering no longer treats the cubemap DDS as a 2D texture. Dedicated
vertex/fragment shaders sample `samplerCUBE`; a per-material sampler override
keeps the source sky bound even when the containing pass uses the dynamic
reflection cube.

## Normal-map contract and tangent space

`R3DMeshAsset` still decodes only fields physically present in the binary
mesh: position, normal and UV. After indices are validated it now reproduces
the old `MeshData::CalcTangentSpace` boundary:

- triangle tangent/bitangent accumulation from UV derivatives;
- Gram-Schmidt tangent orthogonalization;
- handedness correction against the accumulated bitangent;
- deterministic fallback for degenerate UV triangles.

The backend vertex layout carries position, normal, UV, tangent and
bitangent. `ResourceManager::LoadBumpLibMat`'s second sampler is preserved in
`MaterialDefinition`; the active World2 records resolve the shipped
`track1_norm.dds` and `most_norm.dds` resources. The Metal fragment shader
samples that texture and transforms its tangent-space normal into world
space before diffuse/specular/shadow evaluation. The previous diffuse-height
approximation has been removed.

The reflective branch samples the dynamic environment cube with the source
reflect vector and the `reflMapp.fx` 0.4 blend coefficient.

## Connected FxTrail

The loader now imports `world\db\root\ctEffects\trail`, including the source
emitter's:

- `sotDist` one-unit spacing;
- 10-second lifetime;
- 100-point maximum;
- `FxTrailManager` width 0.3;
- fixed Z-up strip orientation.

Wheel behavior type 9 activates the effect while the Jolt vehicle has contact
and is moving. The renderer stores real wheel positions, clears history on a
vehicle reset or discontinuity, prunes by source lifetime/maximum, and uses
the same cached path in cube, reflection and scene passes. Thus a turn
produces a curved historical trail rather than rebuilding a straight
billboard segment from the current heading.

Each path is submitted through the new backend-neutral transient-geometry
API as two vertices per point and six indices per segment. Texture U spans
the strip width and V advances along the path. Alpha/additive state still
comes from the source particle material.

## Observable validation

Renderer telemetry now records:

- begins/draws for all six environment faces;
- draw counts for each original lighting mode;
- successful environment-mapped and normal-mapped submissions;
- transient strip submissions.

`--race-render-smoke-test` fails if a cube face is empty, no reflective draw
sampled the dynamic cube, an expected `glBump` map did not bind its normal
sampler, or no source wheel trail reached the transient backend.

The final arm64 Debug build completed without new warnings. The final Metal
matrix completed with exit code 0:

| Track | Source feature | Environment | Normal map | FxTrail |
| --- | --- | ---: | ---: | ---: |
| 0 / World1 fair | grass/default | 42 | 0 | 168 |
| 16 / World2 rainy | water/reflection/`glBump` | 48 | 768 | 160 |
| 48 / World5 snow | planar reflection | 56 | 0 | 64 |
| 64 / World4 hell | magma/fog | 56 | 0 | 28 |

Every case ran 240 frames, retained four wheel contacts, rendered six cars,
observed both source camera modes, recreated frame targets, and verified the
six cube faces plus Shadow, Scene, HDR, Bloom, Composite and HUD. World2 also
verified Water and Reflection; World5 verified Reflection.

## Remaining differences

This is not a claim of bit-identical D3D9 pixels. The six source camera
contracts and material equations are ported, but no Windows screenshot A/B
was requested for this milestone. Particle scheduling outside FxTrail,
projected shadow details, transparency ordering corner cases, and Jolt/PhysX
numerical differences remain follow-up work.
