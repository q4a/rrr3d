/*
 * The winemetal command-stream ABI: the wire format d9mt's Metal backend
 * batches encoder work into, walked by MTLRenderCommandEncoder_encodeCommands
 * and its compute and blit siblings.
 *
 * These declarations are extracted mechanically from
 * ~/src/d9mt/v2/third_party/winemetal/winemetal.h rather than retyped. They
 * are a wire format: to interoperate at all, every field offset has to match
 * exactly, so transcribing by hand would be all risk and no benefit. Regenerate
 * rather than edit.
 *
 * Origin: DXMT (https://github.com/3Shain/dxmt), vendored into d9mt.
 *
 * The batching exists to amortise crossings of Wine's wow64 boundary. Natively
 * there is no boundary and the indirection buys nothing, but the format is
 * honoured as-is so d9mt's backend links unmodified. Flattening it is a later
 * optimisation, and one the d9mt v2 tree is already pursuing.
 */

#ifndef METALBRIDGE_COMMANDS_H
#define METALBRIDGE_COMMANDS_H

#include "metalbridge.h"

#ifdef __cplusplus
extern "C" {
#endif

enum WMTBarrierScope : uint8_t {
  WMTBarrierScopeBuffers = 1,
  WMTBarrierScopeTextures = 2,
  WMTBarrierScopeRenderTargets = 4,
};

enum WMTBlitCommandType : uint16_t {
  WMTBlitCommandNop,
  WMTBlitCommandCopyFromBufferToBuffer,
  WMTBlitCommandCopyFromBufferToTexture,
  WMTBlitCommandCopyFromTextureToBuffer,
  WMTBlitCommandCopyFromTextureToTexture,
  WMTBlitCommandGenerateMipmaps,
  WMTBlitCommandWaitForFence,
  WMTBlitCommandUpdateFence,
  WMTBlitCommandFillBuffer,
  WMTBlitCommandResolveCounters,
  WMTBlitCommandCopyFromBufferToTextureWithBlitOption,
};

enum WMTComputeCommandType : uint16_t {
  WMTComputeCommandNop,
  WMTComputeCommandDispatch,
  WMTComputeCommandDispatchIndirect,
  WMTComputeCommandSetPSO,
  WMTComputeCommandSetBuffer,
  WMTComputeCommandSetBufferOffset,
  WMTComputeCommandUseResource,
  WMTComputeCommandSetBytes,
  WMTComputeCommandSetTexture,
  WMTComputeCommandDispatchThreads,
  WMTComputeCommandWaitForFence,
  WMTComputeCommandUpdateFence,
  WMTComputeCommandMemoryBarrier,
};

enum WMTCullMode : uint8_t {
  WMTCullModeNone = 0,
  WMTCullModeFront = 1,
  WMTCullModeBack = 2,
};

enum WMTDepthClipMode : uint8_t {
  WMTDepthClipModeClip = 0,
  WMTDepthClipModeClamp = 1,
};

enum WMTIndexType : uint8_t {
  WMTIndexTypeUInt16 = 0,
  WMTIndexTypeUInt32 = 1,
};

enum WMTPrimitiveType : uint8_t {
  WMTPrimitiveTypePoint = 0,
  WMTPrimitiveTypeLine = 1,
  WMTPrimitiveTypeLineStrip = 2,
  WMTPrimitiveTypeTriangle = 3,
  WMTPrimitiveTypeTriangleStrip = 4,
};

enum WMTRenderCommandType : uint16_t {
  WMTRenderCommandNop,
  WMTRenderCommandUseResource,
  WMTRenderCommandSetVertexBuffer,
  WMTRenderCommandSetVertexBufferOffset,
  WMTRenderCommandSetFragmentBuffer,
  WMTRenderCommandSetFragmentBufferOffset,
  WMTRenderCommandSetMeshBuffer,
  WMTRenderCommandSetMeshBufferOffset,
  WMTRenderCommandSetObjectBuffer,
  WMTRenderCommandSetObjectBufferOffset,
  WMTRenderCommandSetFragmentTexture,
  WMTRenderCommandSetFragmentBytes,
  WMTRenderCommandSetRasterizerState,
  WMTRenderCommandSetViewports,
  WMTRenderCommandSetScissorRects,
  WMTRenderCommandSetPSO,
  WMTRenderCommandSetDSSO,
  WMTRenderCommandSetBlendFactorAndStencilRef,
  WMTRenderCommandSetVisibilityMode,
  WMTRenderCommandDraw,
  WMTRenderCommandDrawIndexed,
  WMTRenderCommandDrawIndirect,
  WMTRenderCommandDrawIndexedIndirect,
  WMTRenderCommandDrawMeshThreadgroups,
  WMTRenderCommandDrawMeshThreadgroupsIndirect,
  WMTRenderCommandMemoryBarrier,
  Unused0,
  Unused1,
  Unused2,
  WMTRenderCommandDXMTGeometryDraw,
  WMTRenderCommandDXMTGeometryDrawIndexed,
  WMTRenderCommandDXMTGeometryDrawIndirect,
  WMTRenderCommandDXMTGeometryDrawIndexedIndirect,
  WMTRenderCommandWaitForFence,
  WMTRenderCommandUpdateFence,
  WMTRenderCommandSetViewport,
  WMTRenderCommandSetScissorRect,
  WMTRenderCommandDXMTTessellationMeshDraw,
  WMTRenderCommandDXMTTessellationMeshDrawIndexed,
  WMTRenderCommandDXMTTessellationMeshDrawIndirect,
  WMTRenderCommandDXMTTessellationMeshDrawIndexedIndirect,
  WMTRenderCommandDispatchThreadsPerTile,
};

enum WMTRenderStages : uint8_t {
  WMTRenderStageVertex = 1,
  WMTRenderStageFragment = 2,
  WMTRenderStageTile = 4,
  WMTRenderStageObject = 8,
  WMTRenderStageMesh = 16,
  WMTRenderStagePreRaster = WMTRenderStageVertex | WMTRenderStageObject | WMTRenderStageMesh,
};

enum WMTResourceUsage {
  WMTResourceUsageRead = 1,
  WMTResourceUsageWrite = 2,
  WMTResourceUsageSample = 4,
};

enum WMTTriangleFillMode : uint8_t {
  WMTTriangleFillModeFill = 0,
  WMTTriangleFillModeLines = 1,
};

enum WMTVisibilityResultMode : uint8_t {
  WMTVisibilityResultModeDisabled = 0,
  WMTVisibilityResultModeBoolean = 1,
  WMTVisibilityResultModeCounting = 2,
};

enum WMTWinding : uint8_t {
  WMTWindingClockwise = 0,
  WMTWindingCounterClockwise = 1,
};

struct WMTScissorRect {
  uint64_t x;
  uint64_t y;
  uint64_t width;
  uint64_t height;
};

struct WMTViewport {
  double originX;
  double originY;
  double width;
  double height;
  double znear;
  double zfar;
};

struct wmtcmd_base {
  uint16_t type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
};

struct wmtcmd_blit_nop {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
};

struct wmtcmd_blit_copy_from_buffer_to_buffer {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t src;
  uint64_t src_offset;
  obj_handle_t dst;
  uint64_t dst_offset;
  uint64_t copy_length;
};

struct wmtcmd_blit_copy_from_buffer_to_texture {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t src;
  uint64_t src_offset;
  uint32_t bytes_per_row;
  uint32_t bytes_per_image;
  struct WMTSize size;
  obj_handle_t dst;
  uint32_t slice;
  uint32_t level;
  struct WMTOrigin origin;
};

struct wmtcmd_blit_copy_from_buffer_to_texture_withblitoption {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t src;
  uint64_t src_offset;
  uint32_t bytes_per_row;
  uint32_t bytes_per_image;
  struct WMTSize size;
  obj_handle_t dst;
  uint32_t slice;
  uint16_t level;
  uint16_t options;
  struct WMTOrigin origin;
};

struct wmtcmd_blit_copy_from_texture_to_texture {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t src;
  uint32_t src_slice;
  uint32_t src_level;
  struct WMTOrigin src_origin;
  struct WMTSize src_size;
  obj_handle_t dst;
  uint32_t dst_slice;
  uint32_t dst_level;
  struct WMTOrigin dst_origin;
};

struct wmtcmd_blit_copy_from_texture_to_buffer {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t src;
  uint32_t slice;
  uint32_t level;
  struct WMTOrigin origin;
  struct WMTSize size;
  obj_handle_t dst;
  uint64_t offset;
  uint32_t bytes_per_row;
  uint32_t bytes_per_image;
};

struct wmtcmd_blit_generate_mipmaps {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t texture;
};

struct wmtcmd_blit_fence_op {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t fence;
};

struct wmtcmd_blit_fillbuffer {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t buffer;
  uint64_t offset;
  uint64_t length;
  uint8_t value;
};

struct wmtcmd_blit_resolvecounters {
  enum WMTBlitCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t sample_buffer;
  uint32_t start;
  uint32_t len;
  obj_handle_t dst_buffer;
  uint64_t dst_offset;
};

struct wmtcmd_compute_nop {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
};

struct wmtcmd_compute_dispatch {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTSize size;
};

struct wmtcmd_compute_dispatch_indirect {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
};

struct wmtcmd_compute_setpso {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t pso;
  struct WMTSize threadgroup_size;
};

struct wmtcmd_compute_setbuffer {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t buffer;
  uint64_t offset;
  uint8_t index;
};

struct wmtcmd_compute_setbufferoffset {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t offset;
  uint8_t index;
};

struct wmtcmd_compute_useresource {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t resource;
  enum WMTResourceUsage usage;
};

struct wmtcmd_compute_setbytes {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTMemoryPointer bytes;
  uint64_t length;
  uint8_t index;
};

struct wmtcmd_compute_settexture {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t texture;
  uint8_t index;
};

struct wmtcmd_compute_fence_op {
  enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t fence;
};

struct wmtcmd_compute_memory_barrier {
enum WMTComputeCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTBarrierScope scope;
};

struct wmtcmd_render_nop {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
};

struct wmtcmd_render_useresource {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t resource;
  enum WMTResourceUsage usage;
  enum WMTRenderStages stages;
};

struct wmtcmd_render_setbuffer {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t buffer;
  uint64_t offset;
  uint8_t index;
};

struct wmtcmd_render_setbytes {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTMemoryPointer bytes;
  uint64_t length;
  uint8_t index;
};

struct wmtcmd_render_settexture {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t texture;
  uint8_t index;
};

struct wmtcmd_render_setbufferoffset {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t offset;
  uint8_t index;
};

struct wmtcmd_render_setrasterizerstate {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTTriangleFillMode fill_mode;
  enum WMTCullMode cull_mode;
  enum WMTDepthClipMode depth_clip_mode;
  enum WMTWinding winding;
  float depth_bias;
  float scole_scale;
  float depth_bias_clamp;
};

struct wmtcmd_render_setpso {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t pso;
};

struct wmtcmd_render_setvisibilitymode {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t offset;
  enum WMTVisibilityResultMode mode;
};

struct wmtcmd_render_draw {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTPrimitiveType primitive_type;
  uint64_t vertex_start;
  uint64_t vertex_count;
  uint32_t instance_count;
  uint32_t base_instance;
};

struct wmtcmd_render_draw_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTPrimitiveType primitive_type;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
};

struct wmtcmd_render_draw_indexed {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTPrimitiveType primitive_type;
  enum WMTIndexType index_type;
  uint64_t index_count;
  obj_handle_t index_buffer;
  uint64_t index_buffer_offset;
  uint32_t instance_count;
  int32_t base_vertex;
  uint32_t base_instance;
};

struct wmtcmd_render_draw_indexed_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTPrimitiveType primitive_type;
  enum WMTIndexType index_type;
  obj_handle_t index_buffer;
  uint64_t index_buffer_offset;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
};

struct wmtcmd_render_draw_meshthreadgroups {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTSize threadgroup_per_grid;
  struct WMTSize object_threadgroup_size;
  struct WMTSize mesh_threadgroup_size;
};

struct wmtcmd_render_draw_meshthreadgroups_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
  struct WMTSize object_threadgroup_size;
  struct WMTSize mesh_threadgroup_size;
};

struct wmtcmd_render_memory_barrier {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  enum WMTBarrierScope scope;
  enum WMTRenderStages stages_before;
  enum WMTRenderStages stages_after;
};

struct wmtcmd_render_setviewports {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTMemoryPointer viewports;
  uint8_t viewport_count;
};

struct wmtcmd_render_setviewport {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTViewport viewport;
};

struct wmtcmd_render_setscissorrects {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTMemoryPointer scissor_rects;
  uint8_t rect_count;
};

struct wmtcmd_render_setscissorrect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  struct WMTScissorRect scissor_rect;
};

struct wmtcmd_render_setdsso {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t dsso;
  uint8_t stencil_ref;
};

struct wmtcmd_render_setblendcolor {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  float red;
  float green;
  float blue;
  float alpha;
  uint8_t stencil_ref;
};

struct wmtcmd_render_dxmt_geometry_draw {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t draw_arguments_offset;
  uint32_t warp_count;
  uint32_t instance_count;
  uint32_t vertex_per_warp;
};

struct wmtcmd_render_dxmt_geometry_draw_indexed {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t draw_arguments_offset;
  obj_handle_t index_buffer;
  uint64_t index_buffer_offset;
  uint32_t warp_count;
  uint32_t instance_count;
  uint32_t vertex_per_warp;
};

struct wmtcmd_render_dxmt_geometry_draw_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t imm_draw_arguments;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
  obj_handle_t dispatch_args_buffer;
  uint64_t dispatch_args_offset;
  uint32_t vertex_per_warp;
};

struct wmtcmd_render_dxmt_geometry_draw_indexed_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t index_buffer;
  uint64_t index_buffer_offset;
  obj_handle_t imm_draw_arguments;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
  obj_handle_t dispatch_args_buffer;
  uint64_t dispatch_args_offset;
  uint32_t vertex_per_warp;
};

struct wmtcmd_render_fence_op {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t fence;
  enum WMTRenderStages stages;
};

struct wmtcmd_render_dxmt_tessellation_mesh_draw {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t draw_arguments_offset;
  uint32_t instance_count;
  uint32_t threads_per_patch;
  uint32_t patch_per_group;
  uint32_t patch_per_mesh_instance;
};

struct wmtcmd_render_dxmt_tessellation_mesh_draw_indexed {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint64_t draw_arguments_offset;
  obj_handle_t index_buffer;
  uint64_t index_buffer_offset;
  uint32_t instance_count;
  uint32_t threads_per_patch;
  uint32_t patch_per_group;
  uint32_t patch_per_mesh_instance;
};

struct wmtcmd_render_dxmt_tessellation_mesh_draw_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t imm_draw_arguments;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
  obj_handle_t dispatch_args_buffer;
  uint64_t dispatch_args_offset;
  uint32_t threads_per_patch;
  uint32_t patch_per_group;
};

struct wmtcmd_render_dxmt_tessellation_mesh_draw_indexed_indirect {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  obj_handle_t imm_draw_arguments;
  obj_handle_t indirect_args_buffer;
  uint64_t indirect_args_offset;
  obj_handle_t dispatch_args_buffer;
  uint64_t dispatch_args_offset;
  obj_handle_t index_buffer;
  uint64_t index_buffer_offset;
  uint32_t threads_per_patch;
  uint32_t patch_per_group;
};

struct wmtcmd_render_dispatch_threads_per_tile {
  enum WMTRenderCommandType type;
  uint16_t reserved[3];
  struct WMTMemoryPointer next;
  uint32_t width;
  uint32_t height;
};

/*
 * Each walks a null-terminated linked list of commands via wmtcmd_base::next
 * and replays it onto the encoder.
 */
void MTLRenderCommandEncoder_encodeCommands(obj_handle_t encoder, const struct wmtcmd_base* cmd_head);
void MTLComputeCommandEncoder_encodeCommands(obj_handle_t encoder, const struct wmtcmd_base* cmd_head);
void MTLBlitCommandEncoder_encodeCommands(obj_handle_t encoder, const struct wmtcmd_base* cmd_head);

void MTLCommandEncoder_endEncoding(obj_handle_t encoder);
void MTLCommandEncoder_setLabel(obj_handle_t encoder, const char* label);

#ifdef __cplusplus
}
#endif

#endif /* METALBRIDGE_COMMANDS_H */
