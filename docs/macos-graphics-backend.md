# macOS graphics backend — the surface to implement

## Approach

**Corrected 2026-07-28.** This document previously said the work was to write
a Metal backend behind `DxvkContext` from scratch — the 62 methods below. That
was wrong in an expensive direction: **the implementation already exists.**

`~/src/d9mt` is not a reference. Its `src/d3d9fe/` is **17,539 lines
implementing exactly this contract** — the Metal `DxvkDevice`/`DxvkContext`
behind DXVK's D3D9 front-end — and it is mature enough to run GTA IV at 60fps
at 3K. Its own `docs/METAL-BACKEND-NOTES.md` records the design, including the
handle-smuggling scheme that lets Vulkan's u64 non-dispatchable handles carry
Metal object handles (`VkBuffer` := `MTLBuffer`, `VkImage` := `MTLTexture`,
`VkDeviceAddress` := `MTLBuffer.gpuAddress`, and so on).

There is also a `v2/` tree, 46k lines, rebuilding the backend Metal-native to
shed DXVK's Vulkan-shaped plumbing. v2 is further from done — triangle,
textures, index buffers, blend and depth/stencil work through the real DXVK
frontend — but it is the better long-term shape. **v1 is the one to port
first**; v2 is where it should end up.

### What actually stands between d9mt and a native rrr3d

d9mt targets Wine: it builds as i686 PE and reaches Metal through the
**winemetal** ABI, a flat C interface vendored from DXMT whose Unix side lives
in Wine, not in d9mt. A native build has to replace that boundary — and only
that boundary.

Measured, not estimated:

| Piece | Size |
|---|---|
| winemetal functions d9mt v1 actually calls | **90** of 123 |
| command-stream cases in `encodeCommands` | **66** |
| d9mt v1 Metal backend (reused as-is) | 17,539 lines |

The 90 functions are thin Objective-C wrappers — `MTLCommandBuffer_commit`,
`MTLCommandQueue_commandBuffer`, `MTLTexture_replaceRegion`. The 66 command
cases are a switch over a linked list of `wmtcmd_*` structs, each 1–3 Metal
calls. Both are **fully specified in `winemetal.h`** (225 struct and enum
definitions), so this is transcription against a written contract, not design.

Natively, the batching that command stream exists for — amortising Wine
boundary crossings — is unnecessary. It can be honoured as-is first (simplest,
matches the contract) and flattened later.

### Does d9mt's backend compile natively? Measured: nearly.

Probed 2026-07-28 by compiling `src/d3d9fe/*.cpp` as native arm64 C++20
against the vendored DXVK, this project's XPlatform Win32 substitutes, and
`winemetal.h`. **33 errors across 17,539 lines**, in four groups, none
structural:

| Group | ~Count | What it is |
|---|---|---|
| `MEM_COMMIT` / `MEM_RESERVE` / `MEM_RELEASE` | 6 | `VirtualAlloc` constants; XPlatform's to add |
| Vulkan handle casts | 4 | i686 artifact — handles are u64, pointers were 32-bit. Both are 64-bit natively |
| `D9MT_UnixCall` | 2 | the Wine boundary itself; replaced by calling directly |
| `wsi::Win32WSI` | 1 | window system integration — the SDL shell |

`d9mt_hud.cpp` and `d9mt_watcher.cpp` compile clean already.

No missing Metal work and no D3D9 semantics to write. That is the whole
distance between d9mt's backend and this build.

Two traps worth recording, both of which cost time:

- `-I` paths must reach `vendor/dxvk/include/vulkan/include` and
  `vendor/dxvk/include/spirv/include`, not just `vendor/dxvk/include`.
- `-include xplatform.h` supplies `QueryPerformanceCounter` and friends,
  which d9mt's tracing uses and which XPlatform already implements.

(And a shell trap that produced a wrong answer twice: an unquoted `$INC`
variable of include flags does not word-split in zsh, so the compiler saw
one concatenated argument and reported missing headers that were on the
path. Use an array.)

### Followed through: three errors

Applying the obvious fixes to a scratch copy takes it from 33 to **3 errors
across 17,539 lines**, with `d9mt_context.cpp` -- 5,300 lines and the heart of
the backend -- compiling clean:

```
d9mt_context.cpp     0      d9mt_resources.cpp   0
d9mt_device.cpp      1      d9mt_shader.cpp      0
d9mt_hud.cpp         0      d9mt_watcher.cpp     0
d9mt_instance.cpp    1      d9mt_wsi.cpp         1
d9mt_presenter.cpp   0      stubs.cpp            0
```

The three: two more Vulkan handle casts of the same i686 shape, and
`wsi::Win32WSI`, which is the window system integration and therefore the SDL
shell rather than a backend problem.

What that probe stubbed rather than solved is `D9MT_UnixCall` -- 10 sites. It
dispatches to `d9mtmetal`, a companion library holding the runtime shader
compile (MSL source to metallib), the PSO cache, frame capture and the HUD.
Its implementation is `~/src/d9mt/v2/third_party/d9mtmetal/unix.m`, 723 lines
whose own header comment reads "plain Metal calls, no wine APIs needed". A
native build compiles it directly and replaces the unixlib dispatch with a
call. It also uses sqlite3 for the shader cache.

So the honest remaining list for the backend is:

1. Two handle casts and the surface cast — minutes.
2. `d9mtmetal` compiled natively, `D9MT_UnixCall` replaced by direct calls.
3. `wsi::Win32WSI` replaced by an SDL WSI.

### Order

1. Native `winemetal` implementation in Objective-C++: the 90 functions and 66
   command cases. Bounded and mechanical.
2. Build d9mt v1's `d3d9fe/` and the DXVK D3D9 front-end as native arm64
   rather than i686 PE.
3. Wire `Direct3DCreate9` in `src/XPlatform/source/d3d9_stub.cpp` to it.
4. Then the open question below.

### The one genuine unknown

d9mt never rendered 3D **for this game**, though it runs GTA IV. Whether that
is a backend bug or an artefact of the Wine boundary is still unestablished —
and a native build removes the Wine boundary, so this step also answers it.
`~/src/d9mt/traces/` holds captured frames from this game to diff against.

Everything below this line remains accurate: it is the contract d9mt's
`d3d9fe/` implements, and it is what a native winemetal has to keep standing
up.

## Contract

Derived from DXVK **v3.0.2** by extracting the public `DxvkContext` API and
intersecting it with what `src/d3d9/` actually calls. The full context surface
is 94 methods; D3D9 uses 62 of them. Nothing here needs compute dispatch,
transform feedback, sparse resources, indirect draws or CUDA.

### Frame and command-list lifecycle (5)

```cpp
void beginRecording(const Rc<DxvkCommandList>& cmdList);
void flushCommandList(const VkDebugUtilsLabelEXT* reason, DxvkSubmitStatus* status);
void endFrame();
Rc<DxvkCommandList> beginExternalRendering();
void synchronizeWsi(PresenterSync sync);
```

### Draws (2)

```cpp
void draw(uint32_t count, const VkDrawIndirectCommand* draws);
void drawIndexed(uint32_t count, const VkDrawIndexedIndirectCommand* draws);
```

### Shader and resource binding (10)

```cpp
void bindIndexBuffer(DxvkBufferSlice&& buffer, VkIndexType indexType);
void bindVertexBuffer(uint32_t binding, DxvkBufferSlice&& buffer, uint32_t stride);
void bindVertexBufferRange(uint32_t binding, VkDeviceSize offset, VkDeviceSize length, uint32_t stride);
void bindUniformBuffer(VkShaderStageFlags stages, uint32_t slot, DxvkBufferSlice&& buffer);
void bindUniformBufferRange(VkShaderStageFlags stages, uint32_t slot, VkDeviceSize offset, VkDeviceSize length);
void bindResourceBufferView(VkShaderStageFlags stages, uint32_t slot, Rc<DxvkBufferView>&& view);
void bindResourceImageView(VkShaderStageFlags stages, uint32_t slot, Rc<DxvkImageView>&& view);
void bindResourceSampler(VkShaderStageFlags stages, uint32_t slot, Rc<DxvkSampler>&& sampler);
void pushData(VkShaderStageFlags stages, uint32_t offset, uint32_t size, const void* data);
void setSpecConstants(VkPipelineBindPoint pipeline, uint32_t index, uint32_t count, const void* data);
```

### Render targets and dynamic state (14)

```cpp
void bindRenderTargets(DxvkRenderTargets&& targets, VkImageAspectFlags feedbackLoop);
void setViewports(uint32_t viewportCount, const DxvkViewport* viewports);
void setBlendConstants(DxvkBlendConstants blendConstants);
void setBlendMode(uint32_t attachment, const DxvkBlendMode& blendMode);
void setDepthBias(DxvkDepthBias depthBias);
void setDepthBiasRepresentation(DxvkDepthBiasRepresentation depthBiasRepresentation);
void setDepthBounds(DxvkDepthBounds depthBounds);
void setDepthStencilState(const DxvkDepthStencilState& ds);
void setInputAssemblyState(const DxvkInputAssemblyState& ia);
void setInputLayout(uint32_t attributeCount, const DxvkVertexInput* attributes, uint32_t bindingCount, const DxvkVertexInput* bindings);
void setLogicOpState(const DxvkLogicOpState& lo);
void setMultisampleState(const DxvkMultisampleState& ms);
void setRasterizerState(const DxvkRasterizerState& rs);
void setStencilReference(uint32_t reference);
```

### Clears, copies, blits, resolves (9)

```cpp
void clearImageView(const Rc<DxvkImageView>& imageView, VkOffset3D offset, VkExtent3D extent, VkImageAspectFlags aspect, VkClearValue value);
void clearRenderTarget(const Rc<DxvkImageView>& imageView, VkImageAspectFlags clearAspects, VkClearValue clearValue, VkImageAspectFlags discardAspects);
void copyBuffer(const Rc<DxvkBuffer>& dstBuffer, VkDeviceSize dstOffset, const Rc<DxvkBuffer>& srcBuffer, VkDeviceSize srcOffset, VkDeviceSize numBytes);
void copyBufferToImage(const Rc<DxvkImage>& dstImage, VkImageSubresourceLayers dstSubresource, VkOffset3D dstOffset, VkExtent3D dstExtent, const Rc<DxvkBuffer>& srcBuffer, VkDeviceSize srcOffset, VkDeviceSize rowAlignment, VkDeviceSize sliceAlignment, VkFormat srcFormat);
void copyImage(const Rc<DxvkImage>& dstImage, VkImageSubresourceLayers dstSubresource, VkOffset3D dstOffset, const Rc<DxvkImage>& srcImage, VkImageSubresourceLayers srcSubresource, VkOffset3D srcOffset, VkExtent3D extent);
void copyImageToBuffer(const Rc<DxvkBuffer>& dstBuffer, VkDeviceSize dstOffset, VkDeviceSize rowAlignment, VkDeviceSize sliceAlignment, VkFormat dstFormat, const Rc<DxvkImage>& srcImage, VkImageSubresourceLayers srcSubresource, VkOffset3D srcOffset, VkExtent3D srcExtent);
void blitImageView(const Rc<DxvkImageView>& dstView, const VkOffset3D* dstOffsets, const Rc<DxvkImageView>& srcView, const VkOffset3D* srcOffsets, VkFilter filter);
void resolveImage(const Rc<DxvkImage>& dstImage, const Rc<DxvkImage>& srcImage, const VkImageResolve& region, VkFormat format, VkResolveModeFlagBits mode, VkResolveModeFlagBits stencilMode);
void generateMipmaps(const Rc<DxvkImageView>& imageView, VkFilter filter);
```

### Resource lifetime and upload (6)

```cpp
void initBuffer(const Rc<DxvkBuffer>& buffer);
void initImage(const Rc<DxvkImage>& image, VkImageLayout initialLayout);
void invalidateBuffer(const Rc<DxvkBuffer>& buffer, Rc<DxvkResourceAllocation>&& slice);
void uploadBuffer(const Rc<DxvkBuffer>& buffer, VkDeviceSize bufferOffset, const Rc<DxvkBuffer>& source, VkDeviceSize sourceOffset, VkDeviceSize size);
void transformImage(const Rc<DxvkImage>& dstImage, const VkImageSubresourceRange& dstSubresources, VkImageLayout srcLayout, VkImageLayout dstLayout);
bool ensureImageCompatibility(const Rc<DxvkImage>& image, const DxvkImageUsageInfo& usageInfo);
```

### Queries, sync, barriers (8)

```cpp
void beginQuery(const Rc<DxvkQuery>& query);
void endQuery(const Rc<DxvkQuery>& query);
void writeTimestamp(const Rc<DxvkQuery>& query);
void signal(const Rc<sync::Signal>& signal, uint64_t value);
void signalGpuEvent(const Rc<DxvkEvent>& event);
void emitGraphicsBarrier(VkPipelineStageFlags srcStages, VkAccessFlags srcAccess, VkPipelineStageFlags dstStages, VkAccessFlags dstAccess);
void beginLatencyTracking(const Rc<DxvkLatencyTracker>& tracker, uint64_t frameId);
void endLatencyTracking(const Rc<DxvkLatencyTracker>& tracker);
```

### Debug labels (3)

```cpp
void beginDebugLabel(const VkDebugUtilsLabelEXT& label);
void endDebugLabel();
void insertDebugLabel(const VkDebugUtilsLabelEXT& label);
```

### Via DxvkCommandList, through beginExternalRendering() (5)

```
bindResources, cmdBindPipeline, cmdDispatch, cmdPipelineBarrier, track
```

**57 `DxvkContext` methods, plus 5 command-list methods — 62 distinct.**

## Status of the surrounding pieces

- **Shader path: validated.** DXVK 3.0 deleted `src/dxso/` and replaced it
  with `dxbc-spirv`. All 48 shader entry points this game ships compile
  through DXSO → SPIR-V → MSL → metallib. See `tools/shader-pipeline/`.
- **Backend: not started.** The methods above are the work.
- **D3DX is out of scope for DXVK.** `ID3DXEffect`, `ID3DXFont` and
  `D3DXCreateTextureFromFileEx` are not part of D3D9 and remain ours to
  implement regardless of which backend is chosen.

## References, in order of usefulness

1. **`~/src/d9mt/src/d3d9fe/`** — not a reference, the implementation. 17,539
   lines of exactly this contract, running GTA IV at 60fps.
   `docs/METAL-BACKEND-NOTES.md` beside it records the design decisions.
2. **`~/src/d9mt/v2/third_party/winemetal/winemetal.h`** — the full contract
   for the piece that has to be written: 123 functions, 225 struct and enum
   definitions, the whole command-stream format.
3. **DXVK's own Vulkan backend** (`src/dxvk/dxvk_context.cpp`) — for any
   method where d9mt's Metal implementation is unclear, this shows what the
   semantics must be.
4. `~/src/d9mt/traces/` — captured frames of this game under d9mt, for ground
   truth on the call sequence a real frame produces, and for diffing against
   once something renders.
5. `~/src/d9mt/v2/` — the Metal-native rebuild. Where this should end up, not
   where it should start.
