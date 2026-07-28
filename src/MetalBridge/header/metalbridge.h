/*
 * A native implementation of the `winemetal` ABI.
 *
 * d9mt's Metal backend (~/src/d9mt, src/d3d9fe/ -- 17.5k lines implementing
 * the DxvkContext contract in docs/macos-graphics-backend.md) reaches Metal
 * through a flat C interface called winemetal, vendored there from DXMT. That
 * interface exists to carry Metal calls across Wine's wow64 boundary: the PE
 * side calls thin thunks, the Unix side does the real work.
 *
 * This project has no Wine boundary to cross. Metal is right here. So the same
 * ABI is implemented directly, and the boundary disappears -- there is no Wine
 * dependency in this build and none is being introduced. The name is spelled
 * "MetalBridge" rather than "winemetal" for exactly that reason; d9mt's
 * backend calls these names, and nothing about them is Wine-specific.
 *
 * Handles: winemetal passes objects as opaque uint64. Across the Wine boundary
 * DXMT needs a real indirection. Natively an Objective-C pointer is already 64
 * bits, so the handle IS the pointer, bridged without transferring ownership.
 * Lifetime is manual retain/release, matching what the ABI promises callers.
 *
 * Status: foundation plus the object and device layer. The full surface d9mt
 * v1 uses is 90 functions plus 66 command-stream cases, all specified in
 * ~/src/d9mt/v2/third_party/winemetal/winemetal.h.
 */

#ifndef METALBRIDGE_H
#define METALBRIDGE_H

#include <stdint.h>
#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t obj_handle_t;

#define NULL_OBJECT_HANDLE 0

/* ---- object lifetime ---- */

void NSObject_retain(obj_handle_t obj);
void NSObject_release(obj_handle_t obj);

/* ---- device ---- */

/*
 * The ABI spells this WMTCopyAllDevices, not MTLCopyAllDevices: the latter is
 * a real Metal symbol and would collide natively. Returns an NSArray handle; on Apple
 * silicon there is exactly one device and it is always the default.
 */
obj_handle_t WMTCopyAllDevices(void);

uint64_t NSArray_count(obj_handle_t array);
obj_handle_t NSArray_object(obj_handle_t array, uint64_t index);

obj_handle_t MTLDevice_newCommandQueue(obj_handle_t device, uint64_t maxCommandBufferCount);

/*
 * Returns an NSString handle, not bytes. Callers pair it with
 * NSString_getCString -- which is what d9mt's mtlDevice() does.
 */
obj_handle_t MTLDevice_name(obj_handle_t device);

uint64_t MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device);
bool MTLDevice_hasUnifiedMemory(obj_handle_t device);
bool MTLDevice_supportsFamily(obj_handle_t device, uint32_t family);

/* ---- command queue and buffer ---- */

obj_handle_t MTLCommandQueue_commandBuffer(obj_handle_t queue);

void MTLCommandBuffer_commit(obj_handle_t cmdbuf);
void MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf);
uint32_t MTLCommandBuffer_status(obj_handle_t cmdbuf);

/* ---- resources ----
 *
 * The WMT enums mirror Metal's numeric values exactly -- WMTPixelFormatA8Unorm
 * is 1 as MTLPixelFormatA8Unorm is, RGBA8Unorm 70, BGRA8Unorm 80, and the
 * texture type, usage, resource-option and GPU-family enums line up the same
 * way. DXMT numbered them deliberately, so every conversion below is a cast
 * rather than a lookup table.
 */

/*
 * A pointer, wrapped because the ABI has to survive a 32-bit PE caller holding
 * a 64-bit Unix pointer. Natively there is no such split.
 */
struct WMTMemoryPointer { void* ptr; };
struct WMTConstMemoryPointer { const void* ptr; };

struct WMTOrigin { uint64_t x, y, z; };
struct WMTSize   { uint64_t width, height, depth; };

struct WMTBufferInfo
{
    uint64_t length;      /* in  */
    uint64_t options;     /* in, WMTResourceOptions */
    struct WMTMemoryPointer memory; /* inout: contents pointer */
    uint64_t gpu_address; /* out */
};

static_assert(sizeof(struct WMTBufferInfo) == 32, "WMTBufferInfo must match winemetal.h");

static_assert(sizeof(struct WMTConstMemoryPointer) == 8, "WMTConstMemoryPointer must match winemetal.h");

static_assert(sizeof(struct WMTMemoryPointer) == 8, "WMTMemoryPointer must match winemetal.h");

struct WMTTextureInfo
{
    uint32_t pixel_format;
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t array_length;
    uint32_t type                : 8;
    uint32_t mipmap_level_count  : 8;
    uint32_t sample_count        : 8;
    uint32_t usage               : 8;
    uint64_t options;
    uint32_t reserved;
    uint32_t mach_port;           /* in/out; unused natively */
    uint64_t gpu_resource_id;     /* out */
};

/*
 * Every filter and address-mode field is one byte, not four.
 *
 * The enums these mirror are declared `: uint8_t` in winemetal.h. Widening
 * them to uint32_t here made the struct 48 bytes instead of 32 and shifted
 * everything past the first field, so max_anisotroy read garbage -- Metal's
 * validation layer caught it as "maxAnisotropy value (4094237408) is invalid",
 * sampler creation then failed, and with it every textured draw. The static
 * assert below is the ABI's own; it is mirrored here so the next such slip is
 * a compile error rather than a blank screen.
 */
struct WMTSamplerInfo
{
    uint8_t  min_filter;
    uint8_t  mag_filter;
    uint8_t  mip_filter;
    uint8_t  r_address_mode;
    uint8_t  s_address_mode;
    uint8_t  t_address_mode;
    uint8_t  border_color;
    uint8_t  compare_function;
    float    lod_min_clamp;
    float    lod_max_clamp;
    uint32_t max_anisotroy;       /* spelling is the ABI's */
    bool     normalized_coords;
    bool     lod_average;
    bool     support_argument_buffers;
    uint64_t gpu_resource_id;     /* out */
};

static_assert(sizeof(struct WMTSamplerInfo) == 32, "WMTSamplerInfo must match winemetal.h");

obj_handle_t MTLDevice_newBuffer(obj_handle_t device, struct WMTBufferInfo* info);
obj_handle_t MTLDevice_newTexture(obj_handle_t device, struct WMTTextureInfo* info);
obj_handle_t MTLDevice_newSamplerState(obj_handle_t device, struct WMTSamplerInfo* info);

obj_handle_t MTLBuffer_newTexture(obj_handle_t buffer, struct WMTTextureInfo* info,
                                  uint64_t offset, uint64_t bytes_per_row);

void MTLBuffer_didModifyRange(obj_handle_t buffer, uint64_t start, uint64_t length);
void MTLBuffer_updateContents(obj_handle_t buffer, uint64_t offset,
                              struct WMTConstMemoryPointer data, uint64_t length);

/* MTLTextureSwizzle values, matching Metal's numbering. */
enum WMTTextureSwizzle
{
    WMTTextureSwizzleZero  = 0,
    WMTTextureSwizzleOne   = 1,
    WMTTextureSwizzleRed   = 2,
    WMTTextureSwizzleGreen = 3,
    WMTTextureSwizzleBlue  = 4,
    WMTTextureSwizzleAlpha = 5
};

struct WMTTextureSwizzleChannels
{
    unsigned char r, g, b, a;
};

/*
 * Note the swizzle: it sits between the slice range and the out-pointer. It is
 * passed by value, so omitting it shifts every argument after it -- which is
 * exactly what happened the first time this was written, and what turned
 * out_gpu_resource_id into a wild pointer.
 */
obj_handle_t MTLTexture_newTextureView(obj_handle_t texture, uint32_t format, uint32_t texture_type,
                                       uint16_t level_start, uint16_t level_count,
                                       uint16_t slice_start, uint16_t slice_count,
                                       struct WMTTextureSwizzleChannels swizzle,
                                       uint64_t* out_gpu_resource_id);

void MTLTexture_replaceRegion(obj_handle_t texture, struct WMTOrigin origin, struct WMTSize size,
                              uint64_t level, uint64_t slice, struct WMTMemoryPointer data,
                              uint64_t bytes_per_row, uint64_t bytes_per_image);

uint32_t MTLTexture_pixelFormat(obj_handle_t texture);
uint64_t MTLTexture_width(obj_handle_t texture);
uint64_t MTLTexture_height(obj_handle_t texture);
uint64_t MTLTexture_depth(obj_handle_t texture);
uint64_t MTLTexture_arrayLength(obj_handle_t texture);
uint64_t MTLTexture_mipmapLevelCount(obj_handle_t texture);

uint64_t MTLDevice_minimumLinearTextureAlignmentForPixelFormat(obj_handle_t device, uint32_t format);

#ifdef __cplusplus
}
#endif

#endif /* METALBRIDGE_H */
