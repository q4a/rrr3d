/*
 * Pipeline, depth-stencil and render-pass descriptors from the winemetal ABI.
 *
 * Extracted mechanically from ~/src/d9mt/v2/third_party/winemetal/winemetal.h,
 * for the same reason as metalbridge_commands.h: these are wire formats shared
 * with d9mt's backend and every offset has to match. Regenerate, do not edit.
 *
 * Origin: DXMT (https://github.com/3Shain/dxmt), vendored into d9mt.
 */

#ifndef METALBRIDGE_PIPELINE_H
#define METALBRIDGE_PIPELINE_H

#include "metalbridge.h"
/* WMTWinding, WMTCompareFunction and friends. */
#include "metalbridge_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

enum WMTPixelFormat : uint32_t {
  WMTPixelFormatInvalid = 0,
  WMTPixelFormatA8Unorm = 1,
  WMTPixelFormatR8Unorm = 10,
  WMTPixelFormatR8Unorm_sRGB = 11,
  WMTPixelFormatR8Snorm = 12,
  WMTPixelFormatR8Uint = 13,
  WMTPixelFormatR8Sint = 14,
  WMTPixelFormatR16Unorm = 20,
  WMTPixelFormatR16Snorm = 22,
  WMTPixelFormatR16Uint = 23,
  WMTPixelFormatR16Sint = 24,
  WMTPixelFormatR16Float = 25,
  WMTPixelFormatRG8Unorm = 30,
  WMTPixelFormatRG8Unorm_sRGB = 31,
  WMTPixelFormatRG8Snorm = 32,
  WMTPixelFormatRG8Uint = 33,
  WMTPixelFormatRG8Sint = 34,
  WMTPixelFormatB5G6R5Unorm = 40,
  WMTPixelFormatA1BGR5Unorm = 41,
  WMTPixelFormatABGR4Unorm = 42,
  WMTPixelFormatBGR5A1Unorm = 43,
  WMTPixelFormatR32Uint = 53,
  WMTPixelFormatR32Sint = 54,
  WMTPixelFormatR32Float = 55,
  WMTPixelFormatRG16Unorm = 60,
  WMTPixelFormatRG16Snorm = 62,
  WMTPixelFormatRG16Uint = 63,
  WMTPixelFormatRG16Sint = 64,
  WMTPixelFormatRG16Float = 65,
  WMTPixelFormatRGBA8Unorm = 70,
  WMTPixelFormatRGBA8Unorm_sRGB = 71,
  WMTPixelFormatRGBA8Snorm = 72,
  WMTPixelFormatRGBA8Uint = 73,
  WMTPixelFormatRGBA8Sint = 74,
  WMTPixelFormatBGRA8Unorm = 80,
  WMTPixelFormatBGRA8Unorm_sRGB = 81,
  WMTPixelFormatRGB10A2Unorm = 90,
  WMTPixelFormatRGB10A2Uint = 91,
  WMTPixelFormatRG11B10Float = 92,
  WMTPixelFormatRGB9E5Float = 93,
  WMTPixelFormatBGR10A2Unorm = 94,
  WMTPixelFormatBGR10_XR = 554,
  WMTPixelFormatBGR10_XR_sRGB = 555,
  WMTPixelFormatRG32Uint = 103,
  WMTPixelFormatRG32Sint = 104,
  WMTPixelFormatRG32Float = 105,
  WMTPixelFormatRGBA16Unorm = 110,
  WMTPixelFormatRGBA16Snorm = 112,
  WMTPixelFormatRGBA16Uint = 113,
  WMTPixelFormatRGBA16Sint = 114,
  WMTPixelFormatRGBA16Float = 115,
  WMTPixelFormatBGRA10_XR = 552,
  WMTPixelFormatBGRA10_XR_sRGB = 553,
  WMTPixelFormatRGBA32Uint = 123,
  WMTPixelFormatRGBA32Sint = 124,
  WMTPixelFormatRGBA32Float = 125,
  WMTPixelFormatBC1_RGBA = 130,
  WMTPixelFormatBC1_RGBA_sRGB = 131,
  WMTPixelFormatBC2_RGBA = 132,
  WMTPixelFormatBC2_RGBA_sRGB = 133,
  WMTPixelFormatBC3_RGBA = 134,
  WMTPixelFormatBC3_RGBA_sRGB = 135,
  WMTPixelFormatBC4_RUnorm = 140,
  WMTPixelFormatBC4_RSnorm = 141,
  WMTPixelFormatBC5_RGUnorm = 142,
  WMTPixelFormatBC5_RGSnorm = 143,
  WMTPixelFormatBC6H_RGBFloat = 150,
  WMTPixelFormatBC6H_RGBUfloat = 151,
  WMTPixelFormatBC7_RGBAUnorm = 152,
  WMTPixelFormatBC7_RGBAUnorm_sRGB = 153,
  WMTPixelFormatPVRTC_RGB_2BPP = 160,
  WMTPixelFormatPVRTC_RGB_2BPP_sRGB = 161,
  WMTPixelFormatPVRTC_RGB_4BPP = 162,
  WMTPixelFormatPVRTC_RGB_4BPP_sRGB = 163,
  WMTPixelFormatPVRTC_RGBA_2BPP = 164,
  WMTPixelFormatPVRTC_RGBA_2BPP_sRGB = 165,
  WMTPixelFormatPVRTC_RGBA_4BPP = 166,
  WMTPixelFormatPVRTC_RGBA_4BPP_sRGB = 167,
  WMTPixelFormatEAC_R11Unorm = 170,
  WMTPixelFormatEAC_R11Snorm = 172,
  WMTPixelFormatEAC_RG11Unorm = 174,
  WMTPixelFormatEAC_RG11Snorm = 176,
  WMTPixelFormatEAC_RGBA8 = 178,
  WMTPixelFormatEAC_RGBA8_sRGB = 179,
  WMTPixelFormatETC2_RGB8 = 180,
  WMTPixelFormatETC2_RGB8_sRGB = 181,
  WMTPixelFormatETC2_RGB8A1 = 182,
  WMTPixelFormatETC2_RGB8A1_sRGB = 183,
  WMTPixelFormatASTC_4x4_sRGB = 186,
  WMTPixelFormatASTC_5x4_sRGB = 187,
  WMTPixelFormatASTC_5x5_sRGB = 188,
  WMTPixelFormatASTC_6x5_sRGB = 189,
  WMTPixelFormatASTC_6x6_sRGB = 190,
  WMTPixelFormatASTC_8x5_sRGB = 192,
  WMTPixelFormatASTC_8x6_sRGB = 193,
  WMTPixelFormatASTC_8x8_sRGB = 194,
  WMTPixelFormatASTC_10x5_sRGB = 195,
  WMTPixelFormatASTC_10x6_sRGB = 196,
  WMTPixelFormatASTC_10x8_sRGB = 197,
  WMTPixelFormatASTC_10x10_sRGB = 198,
  WMTPixelFormatASTC_12x10_sRGB = 199,
  WMTPixelFormatASTC_12x12_sRGB = 200,
  WMTPixelFormatASTC_4x4_LDR = 204,
  WMTPixelFormatASTC_5x4_LDR = 205,
  WMTPixelFormatASTC_5x5_LDR = 206,
  WMTPixelFormatASTC_6x5_LDR = 207,
  WMTPixelFormatASTC_6x6_LDR = 208,
  WMTPixelFormatASTC_8x5_LDR = 210,
  WMTPixelFormatASTC_8x6_LDR = 211,
  WMTPixelFormatASTC_8x8_LDR = 212,
  WMTPixelFormatASTC_10x5_LDR = 213,
  WMTPixelFormatASTC_10x6_LDR = 214,
  WMTPixelFormatASTC_10x8_LDR = 215,
  WMTPixelFormatASTC_10x10_LDR = 216,
  WMTPixelFormatASTC_12x10_LDR = 217,
  WMTPixelFormatASTC_12x12_LDR = 218,
  WMTPixelFormatASTC_4x4_HDR = 222,
  WMTPixelFormatASTC_5x4_HDR = 223,
  WMTPixelFormatASTC_5x5_HDR = 224,
  WMTPixelFormatASTC_6x5_HDR = 225,
  WMTPixelFormatASTC_6x6_HDR = 226,
  WMTPixelFormatASTC_8x5_HDR = 228,
  WMTPixelFormatASTC_8x6_HDR = 229,
  WMTPixelFormatASTC_8x8_HDR = 230,
  WMTPixelFormatASTC_10x5_HDR = 231,
  WMTPixelFormatASTC_10x6_HDR = 232,
  WMTPixelFormatASTC_10x8_HDR = 233,
  WMTPixelFormatASTC_10x10_HDR = 234,
  WMTPixelFormatASTC_12x10_HDR = 235,
  WMTPixelFormatASTC_12x12_HDR = 236,
  WMTPixelFormatGBGR422 = 240,
  WMTPixelFormatBGRG422 = 241,
  WMTPixelFormatDepth16Unorm = 250,
  WMTPixelFormatDepth32Float = 252,
  WMTPixelFormatStencil8 = 253,
  WMTPixelFormatDepth24Unorm_Stencil8 = 255,
  WMTPixelFormatDepth32Float_Stencil8 = 260,
  WMTPixelFormatX32_Stencil8 = 261,
  WMTPixelFormatX24_Stencil8 = 262,

  WMTPixelFormatAlphaIsOne = 0x00800000,
  WMTPixelFormatBGRX8Unorm = WMTPixelFormatAlphaIsOne | WMTPixelFormatBGRA8Unorm,
  WMTPixelFormatBGRX8Unorm_sRGB = WMTPixelFormatAlphaIsOne | WMTPixelFormatBGRA8Unorm_sRGB,

  WMTPixelFormatRGB1Swizzle = WMTPixelFormatAlphaIsOne,
  WMTPixelFormatR001Swizzle = 0x00400000,
  WMTPixelFormat0R01Swizzle = 0x00200000,

  WMTPixelFormatR32X8X32 = WMTPixelFormatR001Swizzle | WMTPixelFormatDepth32Float_Stencil8,
  // WMTPixelFormatR24X8 = WMTPixelFormatR001Swizzle | WMTPixelFormatDepth24Unorm_Stencil8,
  WMTPixelFormatX32G8X32 = WMTPixelFormat0R01Swizzle | WMTPixelFormatX32_Stencil8,
  // WMTPixelFormatX24G8 = WMTPixelFormat0R01Swizzle | WMTPixelFormatX24_Stencil8,

  WMTPixelFormatGBARSwizzle = 0x00100000,

  WMTPixelFormatBGRA4Unorm = WMTPixelFormatGBARSwizzle | WMTPixelFormatABGR4Unorm,

  WMTPixelFormatCustomSwizzle = WMTPixelFormatRGB1Swizzle | WMTPixelFormatR001Swizzle | WMTPixelFormat0R01Swizzle | WMTPixelFormatGBARSwizzle,
};

struct WMTClearColor {
  double r;
  double g;
  double b;
  double a;
};

enum WMTLogicOperation : uint8_t {
  WMTLogicOperationClear = 0,
  WMTLogicOperationSet = 1,
  WMTLogicOperationCopy = 2,
  WMTLogicOperationCopyInverted = 3,
  WMTLogicOperationNoOp = 4,
  WMTLogicOperationInvert = 5,
  WMTLogicOperationAnd = 6,
  WMTLogicOperationNand = 7,
  WMTLogicOperationOr = 8,
  WMTLogicOperationNor = 9,
  WMTLogicOperationXor = 10,
  WMTLogicOperationEquiv = 11,
  WMTLogicOperationAndReverse = 12,
  WMTLogicOperationAndInverted = 13,
  WMTLogicOperationOrReverse = 14,
  WMTLogicOperationOrInverted = 15,
};

enum WMTPrimitiveTopologyClass {
  WMTPrimitiveTopologyClassUnspecified = 0,
  WMTPrimitiveTopologyClassPoint = 1,
  WMTPrimitiveTopologyClassLine = 2,
  WMTPrimitiveTopologyClassTriangle = 3,
};

enum WMTTessellationPartitionMode : uint8_t {
  WMTTessellationPartitionModePow2 = 0,
  WMTTessellationPartitionModeInteger = 1,
  WMTTessellationPartitionModeFractionalOdd = 2,
  WMTTessellationPartitionModeFractionalEven = 3,
};

enum WMTTessellationFactorStepFunction : uint8_t {
  WMTTessellationFactorStepFunctionConstant = 0,
  WMTTessellationFactorStepFunctionPerPatch = 1,
  WMTTessellationFactorStepFunctionPerInstance = 2,
  WMTTessellationFactorStepFunctionPerPatchAndPerInstance = 3,
};

enum WMTCompareFunction : uint8_t {
  WMTCompareFunctionNever = 0,
  WMTCompareFunctionLess = 1,
  WMTCompareFunctionEqual = 2,
  WMTCompareFunctionLessEqual = 3,
  WMTCompareFunctionGreater = 4,
  WMTCompareFunctionNotEqual = 5,
  WMTCompareFunctionGreaterEqual = 6,
  WMTCompareFunctionAlways = 7,
};

enum WMTStencilOperation : uint8_t {
  WMTStencilOperationKeep = 0,
  WMTStencilOperationZero = 1,
  WMTStencilOperationReplace = 2,
  WMTStencilOperationIncrementClamp = 3,
  WMTStencilOperationDecrementClamp = 4,
  WMTStencilOperationInvert = 5,
  WMTStencilOperationIncrementWrap = 6,
  WMTStencilOperationDecrementWrap = 7,
};

enum WMTLoadAction {
  WMTLoadActionDontCare = 0,
  WMTLoadActionLoad = 1,
  WMTLoadActionClear = 2,
};

enum WMTStoreAction {
  WMTStoreActionDontCare = 0,
  WMTStoreActionStore = 1,
  WMTStoreActionMultisampleResolve = 2,
  WMTStoreActionStoreAndMultisampleResolve = 3,
  WMTStoreActionUnknown = 4,
  WMTStoreActionCustomSampleDepthStore = 5,
};

enum WMTBlendFactor : uint8_t {
  WMTBlendFactorZero = 0,
  WMTBlendFactorOne = 1,
  WMTBlendFactorSourceColor = 2,
  WMTBlendFactorOneMinusSourceColor = 3,
  WMTBlendFactorSourceAlpha = 4,
  WMTBlendFactorOneMinusSourceAlpha = 5,
  WMTBlendFactorDestinationColor = 6,
  WMTBlendFactorOneMinusDestinationColor = 7,
  WMTBlendFactorDestinationAlpha = 8,
  WMTBlendFactorOneMinusDestinationAlpha = 9,
  WMTBlendFactorSourceAlphaSaturated = 10,
  WMTBlendFactorBlendColor = 11,
  WMTBlendFactorOneMinusBlendColor = 12,
  WMTBlendFactorBlendAlpha = 13,
  WMTBlendFactorOneMinusBlendAlpha = 14,
  WMTBlendFactorSource1Color = 15,
  WMTBlendFactorOneMinusSource1Color = 16,
  WMTBlendFactorSource1Alpha = 17,
  WMTBlendFactorOneMinusSource1Alpha = 18,
};

enum WMTBlendOperation : uint8_t {
  WMTBlendOperationAdd = 0,
  WMTBlendOperationSubtract = 1,
  WMTBlendOperationReverseSubtract = 2,
  WMTBlendOperationMin = 3,
  WMTBlendOperationMax = 4,
};

enum WMTColorWriteMask : uint8_t {
  WMTColorWriteMaskNone = 0,
  WMTColorWriteMaskRed = 8,
  WMTColorWriteMaskGreen = 4,
  WMTColorWriteMaskBlue = 2,
  WMTColorWriteMaskAlpha = 1,
  WMTColorWriteMaskAll = 15,
};

enum WMTSamplerMinMagFilter : uint8_t {
  WMTSamplerMinMagFilterNearest = 0,
  WMTSamplerMinMagFilterLinear = 1,
};

enum WMTSamplerMipFilter : uint8_t {
  WMTSamplerMipFilterNotMipmapped = 0,
  WMTSamplerMipFilterNearest = 1,
  WMTSamplerMipFilterLinear = 2,
};

enum WMTSamplerAddressMode : uint8_t {
  WMTSamplerAddressModeClampToEdge = 0,
  WMTSamplerAddressModeMirrorClampToEdge = 1,
  WMTSamplerAddressModeRepeat = 2,
  WMTSamplerAddressModeMirrorRepeat = 3,
  WMTSamplerAddressModeClampToZero = 4,
  WMTSamplerAddressModeClampToBorderColor = 5,
};

enum WMTSamplerBorderColor : uint8_t {
  WMTSamplerBorderColorTransparentBlack = 0,
  WMTSamplerBorderColorOpaqueBlack = 1,
  WMTSamplerBorderColorOpaqueWhite = 2,
};

struct WMTColorAttachmentBlendInfo {
  enum WMTPixelFormat pixel_format;
  enum WMTBlendOperation rgb_blend_operation;
  enum WMTBlendOperation alpha_blend_operation;
  enum WMTBlendFactor src_rgb_blend_factor;
  enum WMTBlendFactor dst_rgb_blend_factor;
  enum WMTBlendFactor src_alpha_blend_factor;
  enum WMTBlendFactor dst_alpha_blend_factor;
  uint8_t write_mask;
  bool blending_enabled;
};

struct WMTRenderPipelineInfo {
  struct WMTColorAttachmentBlendInfo colors[8];
  bool alpha_to_coverage_enabled;
  bool logic_operation_enabled;
  enum WMTLogicOperation logic_operation;
  bool rasterization_enabled;
  uint8_t raster_sample_count;
  enum WMTPixelFormat depth_pixel_format;
  enum WMTPixelFormat stencil_pixel_format;
  obj_handle_t vertex_function;
  obj_handle_t fragment_function;
  uint32_t immutable_vertex_buffers;
  uint32_t immutable_fragment_buffers;
  enum WMTPrimitiveTopologyClass input_primitive_topology;
  enum WMTTessellationPartitionMode tessellation_partition_mode;
  uint8_t max_tessellation_factor;
  enum WMTWinding tessellation_output_winding_order;
  enum WMTTessellationFactorStepFunction tessellation_factor_step;
  obj_handle_t binary_archive_for_serialization;
  struct WMTConstMemoryPointer binary_archives_for_lookup;
  uint8_t num_binary_archives_for_lookup;
  bool fail_on_binary_archive_miss;
  uint8_t padding[6];
};

struct WMTComputePipelineInfo {
  obj_handle_t compute_function;
  struct WMTConstMemoryPointer binary_archives_for_lookup;
  obj_handle_t binary_archive_for_serialization;
  uint8_t num_binary_archives_for_lookup;
  bool fail_on_binary_archive_miss;
  uint8_t padding;
  bool tgsize_is_multiple_of_sgwidth;
  uint32_t immutable_buffers;
};

struct WMTStencilInfo {
  bool enabled;
  enum WMTStencilOperation depth_stencil_pass_op;
  enum WMTStencilOperation stencil_fail_op;
  enum WMTStencilOperation depth_fail_op;
  enum WMTCompareFunction stencil_compare_function;
  uint8_t write_mask;
  uint8_t read_mask;
};

struct WMTDepthStencilInfo {
  enum WMTCompareFunction depth_compare_function;
  bool depth_write_enabled;
  struct WMTStencilInfo front_stencil;
  struct WMTStencilInfo back_stencil;
};

struct WMTColorAttachmentInfo {
  obj_handle_t texture;
  enum WMTLoadAction load_action;
  enum WMTStoreAction store_action;
  uint16_t level;
  uint16_t slice;
  uint32_t depth_plane;
  struct WMTClearColor clear_color;
  obj_handle_t resolve_texture;
  uint16_t resolve_level;
  uint16_t resolve_slice;
  uint32_t resolve_depth_plane;
};

struct WMTDepthAttachmentInfo {
  obj_handle_t texture;
  enum WMTLoadAction load_action;
  enum WMTStoreAction store_action;
  uint16_t level;
  uint16_t slice;
  uint32_t depth_plane;
  float clear_depth;
};

struct WMTStencilAttachmentInfo {
  obj_handle_t texture;
  enum WMTLoadAction load_action;
  enum WMTStoreAction store_action;
  uint16_t level;
  uint16_t slice;
  uint32_t depth_plane;
  uint8_t clear_stencil;
};

struct WMTRenderPassInfo {
  struct WMTColorAttachmentInfo colors[8];
  struct WMTDepthAttachmentInfo depth;
  struct WMTStencilAttachmentInfo stencil;
  uint8_t default_raster_sample_count;
  uint8_t render_target_array_length;
  uint8_t tile_width;
  uint8_t tile_height;
  uint32_t render_target_height;
  uint32_t render_target_width;
  obj_handle_t visibility_buffer;
};



struct WMTSampleBufferAttachmentInfo {
  obj_handle_t sample_buffer;
  uint64_t start_of_encoder_sample_index;
  uint64_t end_of_encoder_sample_index;
};

/* ---- libraries and functions ---- */

obj_handle_t MTLDevice_newLibrary(obj_handle_t device, const void* bytecode, uint64_t length);
obj_handle_t MTLLibrary_newFunction(obj_handle_t library, const char* name);

/* ---- pipeline state ---- */

obj_handle_t MTLDevice_newRenderPipelineState(obj_handle_t device,
                                              const struct WMTRenderPipelineInfo* info,
                                              obj_handle_t* out_error);
obj_handle_t MTLDevice_newComputePipelineState(obj_handle_t device,
                                               const struct WMTComputePipelineInfo* info,
                                               obj_handle_t* out_error);
obj_handle_t MTLDevice_newDepthStencilState(obj_handle_t device,
                                            const struct WMTDepthStencilInfo* info);

/* ---- encoders ---- */

obj_handle_t MTLCommandBuffer_renderCommandEncoder(obj_handle_t cmdbuf,
                                                   const struct WMTRenderPassInfo* info);
obj_handle_t MTLCommandBuffer_computeCommandEncoder(obj_handle_t cmdbuf, bool concurrent);
obj_handle_t MTLCommandBuffer_blitCommandEncoder(obj_handle_t cmdbuf);

/* ---- synchronisation ---- */

obj_handle_t MTLDevice_newFence(obj_handle_t device);
obj_handle_t MTLDevice_newEvent(obj_handle_t device);
obj_handle_t MTLDevice_newSharedEvent(obj_handle_t device);

void MTLCommandBuffer_encodeSignalEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value);
void MTLCommandBuffer_encodeWaitForEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value);

uint64_t MTLSharedEvent_signaledValue(obj_handle_t event);
void MTLSharedEvent_signalValue(obj_handle_t event, uint64_t value);
bool MTLSharedEvent_waitUntilSignaledValue(obj_handle_t event, uint64_t value, uint64_t timeout_ms);

/* ---- misc device queries d9mt uses ---- */

uint64_t MTLDevice_currentAllocatedSize(obj_handle_t device);
uint64_t MTLDevice_registryID(obj_handle_t device);
bool MTLDevice_supportsTextureSampleCount(obj_handle_t device, uint64_t count);
bool MTLDevice_supportsBCTextureCompression(obj_handle_t device);
void MTLDevice_setShouldMaximizeConcurrentCompilation(obj_handle_t device, bool value);

enum WMTDataType : uint16_t {
  WMTDataTypeNone = 0,

  WMTDataTypeStruct = 1,
  WMTDataTypeArray = 2,

  WMTDataTypeFloat = 3,
  WMTDataTypeFloat2 = 4,
  WMTDataTypeFloat3 = 5,
  WMTDataTypeFloat4 = 6,

  WMTDataTypeFloat2x2 = 7,
  WMTDataTypeFloat2x3 = 8,
  WMTDataTypeFloat2x4 = 9,

  WMTDataTypeFloat3x2 = 10,
  WMTDataTypeFloat3x3 = 11,
  WMTDataTypeFloat3x4 = 12,

  WMTDataTypeFloat4x2 = 13,
  WMTDataTypeFloat4x3 = 14,
  WMTDataTypeFloat4x4 = 15,

  WMTDataTypeHalf = 16,
  WMTDataTypeHalf2 = 17,
  WMTDataTypeHalf3 = 18,
  WMTDataTypeHalf4 = 19,

  WMTDataTypeHalf2x2 = 20,
  WMTDataTypeHalf2x3 = 21,
  WMTDataTypeHalf2x4 = 22,

  WMTDataTypeHalf3x2 = 23,
  WMTDataTypeHalf3x3 = 24,
  WMTDataTypeHalf3x4 = 25,

  WMTDataTypeHalf4x2 = 26,
  WMTDataTypeHalf4x3 = 27,
  WMTDataTypeHalf4x4 = 28,

  WMTDataTypeInt = 29,
  WMTDataTypeInt2 = 30,
  WMTDataTypeInt3 = 31,
  WMTDataTypeInt4 = 32,

  WMTDataTypeUInt = 33,
  WMTDataTypeUInt2 = 34,
  WMTDataTypeUInt3 = 35,
  WMTDataTypeUInt4 = 36,

  WMTDataTypeShort = 37,
  WMTDataTypeShort2 = 38,
  WMTDataTypeShort3 = 39,
  WMTDataTypeShort4 = 40,

  WMTDataTypeUShort = 41,
  WMTDataTypeUShort2 = 42,
  WMTDataTypeUShort3 = 43,
  WMTDataTypeUShort4 = 44,

  WMTDataTypeChar = 45,
  WMTDataTypeChar2 = 46,
  WMTDataTypeChar3 = 47,
  WMTDataTypeChar4 = 48,

  WMTDataTypeUChar = 49,
  WMTDataTypeUChar2 = 50,
  WMTDataTypeUChar3 = 51,
  WMTDataTypeUChar4 = 52,

  WMTDataTypeBool = 53,
  WMTDataTypeBool2 = 54,
  WMTDataTypeBool3 = 55,
  WMTDataTypeBool4 = 56,
};

struct WMTFunctionConstant {
  struct WMTConstMemoryPointer data;
  enum WMTDataType type;
  uint16_t index;
  uint32_t reserved;
};

enum WMTStringEncoding : uint64_t {
  WMTASCIIStringEncoding = 1,
  WMTNEXTSTEPStringEncoding = 2,
  WMTJapaneseEUCStringEncoding = 3,
  WMTUTF8StringEncoding = 4,
  WMTWMTISOLatin1StringEncoding = 5,
  SymbolStringEncoding = 6,
  WMTNonLossyASCIIStringEncoding = 7,
  WMTShiftJISStringEncoding = 8,
  WMTISOLatin2StringEncoding = 9,
  WMTUnicodeStringEncoding = 10,
  WMTWindowsCP1251StringEncoding = 11,
  WMTWindowsCP1252StringEncoding = 12,
  WMTWindowsCP1253StringEncoding = 13,
  WMTWindowsCP1254StringEncoding = 14,
  WMTWindowsCP1250StringEncoding = 15,
  WMTISO2022JPStringEncoding = 21,
  WMTMacOSRomanStringEncoding = 30,

  WMTUTF16StringEncoding = WMTUnicodeStringEncoding,

  WMTUTF16BigEndianStringEncoding = 0x90000100,
  WMTUTF16LittleEndianStringEncoding = 0x94000100,

  WMTUTF32StringEncoding = 0x8c000100,
  WMTUTF32BigEndianStringEncoding = 0x98000100,
  WMTUTF32LittleEndianStringEncoding = 0x9c000100
};

struct WMTLayerProps
{
    obj_handle_t device;
    double contents_scale;
    double drawable_width;
    double drawable_height;
    bool opaque;
    bool display_sync_enabled;
    bool framebuffer_only;
    uint32_t pixel_format;
};

enum WMTCommandBufferProperty
{
    WMTCommandBufferPropertyKernelStartTime,
    WMTCommandBufferPropertyKernelEndTime,
    WMTCommandBufferPropertyGPUStartTime,
    WMTCommandBufferPropertyGPUEndTime
};

obj_handle_t MTLLibrary_newFunctionWithConstants(obj_handle_t library, const char* name,
    const struct WMTFunctionConstant* constants, uint32_t num_constants, obj_handle_t* err_out);

obj_handle_t MTLCommandBuffer_error(obj_handle_t cmdbuf);
uint64_t MTLCommandBuffer_property(obj_handle_t cmdbuf, enum WMTCommandBufferProperty prop);

obj_handle_t NSAutoreleasePool_alloc_init(void);
obj_handle_t NSObject_description(obj_handle_t obj);
uint64_t NSString_getCString(obj_handle_t str, char* buffer, uint64_t maxLength, uint32_t encoding);

/* ---- presentation ---- */

obj_handle_t CreateMetalViewFromHWND(intptr_t hwnd, obj_handle_t device, obj_handle_t* layer);
void ReleaseMetalView(obj_handle_t view);

void MetalLayer_getProps(obj_handle_t layer, struct WMTLayerProps* props);
void MetalLayer_setProps(obj_handle_t layer, const struct WMTLayerProps* props);
obj_handle_t MetalLayer_nextDrawable(obj_handle_t layer);
obj_handle_t MetalDrawable_texture(obj_handle_t drawable);

void MTLCommandBuffer_presentDrawable(obj_handle_t cmdbuf, obj_handle_t drawable);

#ifdef __cplusplus
}
#endif

#endif /* METALBRIDGE_PIPELINE_H */
