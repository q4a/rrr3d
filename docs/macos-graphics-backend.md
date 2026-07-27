# macOS graphics backend — the surface to implement

## Approach

Keep **DXVK's D3D9 front-end** and put a Metal backend behind `DxvkContext`,
rather than reimplementing Direct3D 9. The front-end already handles the D3D9
semantics this engine leans on — fixed-function emulation, `DrawPrimitiveUP`,
state blocks, the shader constant register file, alpha test — and getting
those right independently would be months of work.

This avoids Vulkan entirely, which matters: stock DXVK requires
`geometryShader` and `shaderCullDistance`, and MoltenVK reports both as
unsupported because Metal has neither. A Metal backend never touches
MoltenVK, so those requirements do not apply.

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

1. **DXVK's own Vulkan backend** (`src/dxvk/dxvk_context.cpp`) — a complete,
   working implementation of exactly this interface. For every method, it
   shows what the semantics must be.
2. `d9mt` — a prior D3D9-on-Metal attempt for this game, built against DXVK
   2.7.1 for Wine. Its `d3d9fe/` is ~13k lines of the same mapping, and its
   `docs/METAL-BACKEND-NOTES.md` records the decisions. The Wine coupling
   (`winemetal` bridge, unixlib) is what a native port drops.
3. The captured traces of this game under d9mt, for ground truth on the call
   sequence a real frame produces.
