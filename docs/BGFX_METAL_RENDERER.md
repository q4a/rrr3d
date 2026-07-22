# Renderer abstraction: corrected Milestone 5

## Scope

Milestone 5 is a renderer migration slice, not a replacement game. It contains
no menu, race, physics, AI, balance or invented gameplay. Its only runtime
scene is a static reconstruction of the original Buggi visual object from the
shipped Motor Rock resources.

The earlier cube/checkerboard scene was not a valid proof of the porting path.
It exercised bgfx, but bypassed the game's resource format and material data.
It has been removed from the Milestone 5 entry point.

## Data path

The corrected path is:

```text
ResourceFileSystem
  -> R3DMeshAsset decoder
      -> legacy R3DMeshFile adapter -> D3D9 MeshData/IndexedVBMesh (Windows)
      -> StaticMeshVertex + material ranges -> GraphicsDevice -> bgfx/Metal
```

`R3DMeshAsset` decodes the actual version-0 `.r3d` binary format:

- coordinate-system and texture-coordinate flags;
- position, normal and UV vertex streams;
- 32-bit triangle indices;
- material IDs and face ranges;
- model bounds;
- strict index/range/end-of-file validation.

The Windows adapter now populates the existing `res::MeshData` from the same
decoder. The original `D3D9RenderDriver`, `GraphManager` and gameplay code are
preserved. Porting their remaining draw passes is subsequent renderer work;
they are not replaced by a parallel game implementation.

## Static scene source

The scene uses only paths already declared by the original
`ResourceManager::LoadCars` and `DataBase::LoadCars` code:

| Resource | Use |
| --- | --- |
| `Data/Car/buggi.r3d` | body positions, normals, UVs, 32-bit indices and six material groups |
| `Data/Car/buggiWheel.r3d` | wheel geometry and three material groups |
| `Data/Car/buggiWheel.txt` | the four original wheel offsets |
| `Data/Car/buggi.dds` | original 600x600 DXT3 texture and mip chain |

The body uses its original identity model transform. Four wheels use the
stored offsets. Wheels on the negative Y side reproduce the original mirrored
Y transform and opposite culling mode from `DataBase::AddWheel`.

The camera and clear colour belong only to this static renderer test. They are
not written into game state and do not define new gameplay.

## Portable API exercised

`renderer::GraphicsDevice` remains free of D3D9, Cocoa, Metal and bgfx types.
This slice exercises:

- Metal swap chain creation and resize;
- vertex buffers with position/normal/UV layout;
- 32-bit index buffers;
- material draw ranges;
- encoded DDS texture upload without replacing it with generated pixels;
- Metal-profile vertex and fragment shaders;
- camera/model matrices;
- depth test/write and clockwise/counter-clockwise culling;
- frame presentation.

Unused future types such as render targets, post-processing, fonts and effects
remain interface boundaries only. Their migration is not claimed by this
milestone.

## D3D9 to bgfx mapping for this slice

| Existing D3D9 path | Internal data/API | bgfx/Metal path |
| --- | --- | --- |
| `R3DMeshFile` | `R3DMeshAsset` | the same decoded mesh data |
| `CreateVertexBuffer` + FVF XYZ/NORMAL/TEX1 | `PositionNormalTexcoord` | `bgfx::VertexLayout` + `createVertexBuffer` |
| `CreateIndexBuffer(D3DFMT_INDEX32)` | `IndexType::UInt32` | `BGFX_BUFFER_INDEX32` |
| `DrawIndexedPrimitive` subsets | `R3DMaterialGroup` / `DrawRange` | ranged `setIndexBuffer` + `submit` |
| `D3DXCreateTextureFromFileInMemory` | encoded texture container | `bgfx::createTexture` for DDS |
| world/view/projection transforms | `Transform` / `Camera` | `setTransform` / `setViewTransform` |
| `D3DRS_CULLMODE`, Z states | `PipelineState` | `BGFX_STATE_CULL_*`, depth/write flags |
| `Present` | `GraphicsDevice::endFrame` | `bgfx::frame` |

## Build and verification

```bash
cmake --preset macos-arm64-m5
cmake --build --preset macos-arm64-m5 -j 8
build/macos-arm64-m5/Debug/RRR3d --verify-assets
build/macos-arm64-m5/Debug/RRR3d --smoke-test-frames=120
```

`--verify-assets` runs without creating a window and fails if a required
resource is missing, has wrong case, contains malformed `.r3d` data or has an
invalid DDS header. The render smoke must report `bgfx/Metal` and complete the
requested number of frames.

## Honest status

Corrected Milestone 5 is complete as the first real-resource renderer slice.
It is not a full `GraphManager` port. In particular, the legacy shader corpus,
all material modes, scene graph, render targets, post-processing, fonts and
effects have not yet crossed the abstraction.

The old portable menu/race implementation from later milestone attempts is not
evidence that those systems are ported. Milestone 6 must continue from the
original `ResourceManager`, GUI and menu data flow; it must not promote that
vertical slice back into the product path.
