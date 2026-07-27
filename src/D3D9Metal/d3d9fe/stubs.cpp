// d9mt: link stubs for the DXVK v2.7.1 d3d9 front-end build (d3d9fe.dll).
//
// Every backend symbol (DxvkContext / DxvkDevice / resources / presenter /
// ...) referenced by the vendored front-end resolves here to a loud abort.
// PE ld resolves symbols BEFORE --gc-sections, so even dead-path references
// need definitions.
//
// Pattern: include the real vendored headers and define the declared
// methods (no mangled-name hacks).  Bodies log the calling symbol and
// abort().  These get replaced one class at a time by the real Metal
// backend.

#include <cstdio>
#include <cstdlib>

#include "../dxvk/src/dxvk/dxvk_access.h"
#include "../dxvk/src/dxvk/dxvk_adapter.h"
#include "../dxvk/src/dxvk/dxvk_buffer.h"
#include "../dxvk/src/dxvk/dxvk_cmdlist.h"
#include "../dxvk/src/dxvk/dxvk_compute.h"
#include "../dxvk/src/dxvk/dxvk_context.h"
#include "../dxvk/src/dxvk/dxvk_descriptor_heap.h"
#include "../dxvk/src/dxvk/dxvk_descriptor_pool.h"
#include "../dxvk/src/dxvk/dxvk_device.h"
#include "../dxvk/src/dxvk/dxvk_fence.h"
#include "../dxvk/src/dxvk/dxvk_graphics.h"
#include "../dxvk/src/dxvk/dxvk_meta_blit.h"
#include "../dxvk/src/dxvk/dxvk_meta_clear.h"
#include "../dxvk/src/dxvk/dxvk_meta_copy.h"
#include "../dxvk/src/dxvk/dxvk_meta_resolve.h"
#include "../dxvk/src/dxvk/dxvk_gpu_event.h"
#include "../dxvk/src/dxvk/dxvk_gpu_query.h"
#include "../dxvk/src/dxvk/dxvk_image.h"
#include "../dxvk/src/dxvk/dxvk_instance.h"
#include "../dxvk/src/dxvk/dxvk_latency.h"
#include "../dxvk/src/dxvk/dxvk_memory.h"
#include "../dxvk/src/dxvk/dxvk_pipemanager.h"
#include "../dxvk/src/dxvk/dxvk_presenter.h"
#include "../dxvk/src/dxvk/dxvk_queue.h"
#include "../dxvk/src/dxvk/dxvk_sampler.h"
#include "../dxvk/src/dxvk/dxvk_sparse.h"
#include "../dxvk/src/dxvk/dxvk_staging.h"
#include "../dxvk/src/dxvk/dxvk_swapchain_blitter.h"
#include "../dxvk/src/dxvk/hud/dxvk_hud.h"
#include "../dxvk/src/vulkan/vulkan_loader.h"
#include "../dxvk/src/wsi/wsi_edid.h"

// Logs the hit both to stderr AND to d3d9fe-stub.log in the process cwd
// (winewrapper swallows stderr, the file survives the abort).
#define D9MT_STUB_BODY                                                        \
  {                                                                           \
    std::fprintf(stderr, "d3d9fe stub hit: %s\n", __PRETTY_FUNCTION__);       \
    std::fflush(stderr);                                                      \
    if (std::FILE* f = std::fopen("d3d9fe-stub.log", "a")) {                  \
      std::fprintf(f, "%s\n", __PRETTY_FUNCTION__);                           \
      std::fclose(f);                                                         \
    }                                                                         \
    std::abort();                                                             \
  }

namespace dxvk {

  // --------------------------------------------------------------- adapter
  // (instance/adapter/capabilities live in d9mt_instance.cpp,
  //  createDevice + the DxvkDevice cascade in d9mt_device.cpp)

  // ---------------------------------------------------------------- device
  // (resource factories live in d9mt_resources.cpp)

  // --------------------------------------------------------------- context
  // (DxvkContext + DxvkCommandList + the draw path live in d9mt_context.cpp,
  //  shader translation in d9mt_shader.cpp)

  // -------------------------------------------------------------- resources
  // (implemented in d9mt_resources.cpp; only the descriptor-buffer-path
  //  DxvkDescriptorUpdateList builder remains a stub — never constructed)
  DxvkDescriptorUpdateList::DxvkDescriptorUpdateList(DxvkDevice* device,
    uint32_t setSize, uint32_t descriptorCount,
    const DxvkDescriptorUpdateInfo* descriptorInfos) D9MT_STUB_BODY

  // -------------------------------------------------------- queries/events
  // (alloc/recycle pools live in d9mt_device.cpp; DxvkEvent::test,
  //  DxvkQuery::getData and the GpuQueryManager visibility-slot machinery
  //  live in d9mt_context.cpp)

  // ---------------------------------- never-constructed Vulkan-only classes
  // Referenced by member/dtor chains of real objects (command list descriptor
  // pools, pipeline-manager maps, DxvkObjects Lazy<Meta*>), but no instance
  // is ever created on the Metal backend; constructors are not defined.
  DxvkCommandPool::~DxvkCommandPool() D9MT_STUB_BODY
  DxvkComputePipeline::~DxvkComputePipeline() D9MT_STUB_BODY
  DxvkGraphicsPipeline::~DxvkGraphicsPipeline() D9MT_STUB_BODY
  DxvkGraphicsPipelineVertexInputLibrary::~DxvkGraphicsPipelineVertexInputLibrary() D9MT_STUB_BODY
  DxvkGraphicsPipelineFragmentOutputLibrary::~DxvkGraphicsPipelineFragmentOutputLibrary() D9MT_STUB_BODY
  DxvkDescriptorPool::~DxvkDescriptorPool() D9MT_STUB_BODY
  DxvkDescriptorPoolSet::~DxvkDescriptorPoolSet() D9MT_STUB_BODY
  DxvkFence::~DxvkFence() D9MT_STUB_BODY
  DxvkMetaBlitObjects::~DxvkMetaBlitObjects() D9MT_STUB_BODY
  DxvkMetaClearObjects::~DxvkMetaClearObjects() D9MT_STUB_BODY
  DxvkMetaCopyObjects::~DxvkMetaCopyObjects() D9MT_STUB_BODY
  DxvkMetaResolveObjects::~DxvkMetaResolveObjects() D9MT_STUB_BODY
  DxvkResourceDescriptorHeap::~DxvkResourceDescriptorHeap() D9MT_STUB_BODY
  DxvkSparseBindSubmission::~DxvkSparseBindSubmission() D9MT_STUB_BODY

  // --------------------------------------------------------------- present
  // (Presenter + DxvkSwapchainBlitter + hud::Hud::createHud live in
  //  d9mt_presenter.cpp)

  // ------------------------------------------------------------------- hud
  // (createHud returns nullptr, so no Hud/HudItem object is ever
  //  constructed; the remaining symbols are dead dtor-chain references)
  namespace hud {
    Hud::~Hud() D9MT_STUB_BODY
    HudItem::~HudItem() D9MT_STUB_BODY
    void HudItem::update(dxvk::high_resolution_clock::time_point time) D9MT_STUB_BODY
    HudClientApiItem::HudClientApiItem(std::string api) D9MT_STUB_BODY
    HudClientApiItem::~HudClientApiItem() D9MT_STUB_BODY
    HudPos HudClientApiItem::render(const Rc<DxvkCommandList>& ctx,
      const HudPipelineKey& key, const HudOptions& options,
      HudRenderer& renderer, HudPos position) D9MT_STUB_BODY
    HudLatencyItem::HudLatencyItem() D9MT_STUB_BODY
    HudLatencyItem::~HudLatencyItem() D9MT_STUB_BODY
    void HudLatencyItem::accumulateStats(const DxvkLatencyStats& stats) D9MT_STUB_BODY
    void HudLatencyItem::update(dxvk::high_resolution_clock::time_point time) D9MT_STUB_BODY
    HudPos HudLatencyItem::render(const Rc<DxvkCommandList>& ctx,
      const HudPipelineKey& key, const HudOptions& options,
      HudRenderer& renderer, HudPos position) D9MT_STUB_BODY
    void HudRenderer::drawText(uint32_t size, HudPos pos, uint32_t color,
      const std::string& text) D9MT_STUB_BODY
  } // namespace hud

  // ---------------------------------------------------------------- vulkan
  // (LibraryLoader/InstanceLoader/DeviceLoader/DeviceFn — the fake dispatch —
  //  live in d9mt_device.cpp; InstanceFn + the LibraryLoader loader-proc
  //  ctor back DxvkAdapter::vki() in d9mt_instance.cpp; LibraryFn is never
  //  constructed)
  namespace vk {
    LibraryFn::~LibraryFn() D9MT_STUB_BODY
  } // namespace vk

  // ------------------------------------------------------------------- wsi
  namespace wsi {
    // d9mt: real (non-abort) impl per BACKEND-SURFACE.md §6.2 — replaces
    // wsi_edid.cpp, which hard-depends on libdisplay-info.
    std::optional<WsiDisplayMetadata> parseColorimetryInfo(
      const std::vector<uint8_t>& edid) {
      return std::nullopt;
    }
  } // namespace wsi

} // namespace dxvk
