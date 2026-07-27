/*
 * Native Metal implementation of the winemetal ABI. See metalbridge.h for why
 * this exists and why it involves no Wine.
 *
 * Every function here is the body DXMT runs on the Unix side of the Wine
 * boundary, called directly instead of through a thunk. Handles are Objective-C
 * pointers, bridged without transferring ownership -- ARC is off for this file
 * so retain/release match the ABI's manual contract exactly.
 */

#include "metalbridge.h"

#import <Metal/Metal.h>
#import <Foundation/Foundation.h>

#include <cstring>

namespace
{

/*
 * A handle is the object pointer. Across the Wine boundary DXMT needs a real
 * indirection because the PE side cannot hold a Unix pointer; here there is no
 * boundary, so the cast is the whole mapping.
 */
template <class _Obj> inline _Obj Unwrap(obj_handle_t handle)
{
	return (__bridge _Obj)(void*)handle;
}

inline obj_handle_t Wrap(id obj)
{
	return (obj_handle_t)(__bridge void*)obj;
}

}

void NSObject_retain(obj_handle_t obj)
{
	if (obj)
		[Unwrap<id>(obj) retain];
}

void NSObject_release(obj_handle_t obj)
{
	if (obj)
		[Unwrap<id>(obj) release];
}

obj_handle_t WMTCopyAllDevices(void)
{
	/*
	 * MTLCopyAllDevices is macOS-only and absent on Apple silicon's unified
	 * stack in some configurations, so this goes through the default device and
	 * wraps it in an array. One GPU, and it is always the right one.
	 */
	id<MTLDevice> device = MTLCreateSystemDefaultDevice();
	if (!device)
		return NULL_OBJECT_HANDLE;

	NSArray* devices = [[NSArray alloc] initWithObjects:device, nil];
	[device release];
	return Wrap(devices);
}

uint64_t NSArray_count(obj_handle_t array)
{
	return array ? [Unwrap<NSArray*>(array) count] : 0;
}

obj_handle_t NSArray_object(obj_handle_t array, uint64_t index)
{
	if (!array)
		return NULL_OBJECT_HANDLE;

	NSArray* nsArray = Unwrap<NSArray*>(array);
	if (index >= [nsArray count])
		return NULL_OBJECT_HANDLE;

	return Wrap([nsArray objectAtIndex:static_cast<NSUInteger>(index)]);
}

obj_handle_t MTLDevice_newCommandQueue(obj_handle_t device, uint64_t maxCommandBufferCount)
{
	if (!device)
		return NULL_OBJECT_HANDLE;

	id<MTLDevice> mtlDevice = Unwrap<id<MTLDevice>>(device);
	id<MTLCommandQueue> queue = maxCommandBufferCount > 0
		? [mtlDevice newCommandQueueWithMaxCommandBufferCount:static_cast<NSUInteger>(maxCommandBufferCount)]
		: [mtlDevice newCommandQueue];

	return Wrap(queue);
}

uint64_t MTLDevice_name(obj_handle_t device, char* buffer, uint64_t size)
{
	if (!device)
		return 0;

	const char* name = [[Unwrap<id<MTLDevice>>(device) name] UTF8String];
	if (!name)
		return 0;

	const uint64_t needed = std::strlen(name) + 1;
	if (buffer && size > 0)
	{
		const uint64_t copied = needed < size ? needed : size;
		std::memcpy(buffer, name, copied);
		buffer[copied - 1] = '\0';
	}
	return needed;
}

uint64_t MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device)
{
	return device ? [Unwrap<id<MTLDevice>>(device) recommendedMaxWorkingSetSize] : 0;
}

uint8_t MTLDevice_hasUnifiedMemory(obj_handle_t device)
{
	return device && [Unwrap<id<MTLDevice>>(device) hasUnifiedMemory] ? 1 : 0;
}

uint8_t MTLDevice_supportsFamily(obj_handle_t device, uint32_t family)
{
	if (!device)
		return 0;

	return [Unwrap<id<MTLDevice>>(device) supportsFamily:static_cast<MTLGPUFamily>(family)] ? 1 : 0;
}

obj_handle_t MTLCommandQueue_commandBuffer(obj_handle_t queue)
{
	if (!queue)
		return NULL_OBJECT_HANDLE;

	/*
	 * Retained because the ABI hands ownership to the caller, where Metal's own
	 * commandBuffer is autoreleased. Callers pair this with NSObject_release.
	 */
	id<MTLCommandBuffer> cmdbuf = [[Unwrap<id<MTLCommandQueue>>(queue) commandBuffer] retain];
	return Wrap(cmdbuf);
}

void MTLCommandBuffer_commit(obj_handle_t cmdbuf)
{
	if (cmdbuf)
		[Unwrap<id<MTLCommandBuffer>>(cmdbuf) commit];
}

void MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf)
{
	if (cmdbuf)
		[Unwrap<id<MTLCommandBuffer>>(cmdbuf) waitUntilCompleted];
}

uint32_t MTLCommandBuffer_status(obj_handle_t cmdbuf)
{
	return cmdbuf ? static_cast<uint32_t>([Unwrap<id<MTLCommandBuffer>>(cmdbuf) status]) : 0;
}

/* ---- resources ----
 *
 * Every WMT enum below is cast straight to its Metal counterpart: DXMT gave
 * them Metal's own numeric values, so WMTPixelFormatBGRA8Unorm is 80 exactly as
 * MTLPixelFormatBGRA8Unorm is. Verified against MTLPixelFormat.h rather than
 * assumed.
 */

namespace
{

MTLTextureDescriptor* DescribeTexture(const WMTTextureInfo& info)
{
	MTLTextureDescriptor* desc = [[MTLTextureDescriptor alloc] init];
	desc.pixelFormat = static_cast<MTLPixelFormat>(info.pixel_format);
	desc.width = info.width;
	desc.height = info.height;
	desc.depth = info.depth;
	desc.arrayLength = info.array_length;
	desc.textureType = static_cast<MTLTextureType>(info.type);
	desc.mipmapLevelCount = info.mipmap_level_count ? info.mipmap_level_count : 1;
	desc.sampleCount = info.sample_count ? info.sample_count : 1;
	desc.usage = static_cast<MTLTextureUsage>(info.usage);
	desc.resourceOptions = static_cast<MTLResourceOptions>(info.options);
	return desc;
}

}

obj_handle_t MTLDevice_newBuffer(obj_handle_t device, struct WMTBufferInfo* info)
{
	if (!device || !info)
		return NULL_OBJECT_HANDLE;

	id<MTLDevice> mtlDevice = Unwrap<id<MTLDevice>>(device);
	const MTLResourceOptions options = static_cast<MTLResourceOptions>(info->options);

	/*
	 * A non-null memory pointer means "wrap this allocation" rather than
	 * "allocate". Metal needs it page-aligned, and the no-copy variant wants a
	 * deallocator; passing nil leaves ownership with the caller, which is what
	 * the ABI means.
	 */
	id<MTLBuffer> buffer = info->memory.ptr
		? [mtlDevice newBufferWithBytesNoCopy:info->memory.ptr
		                               length:info->length
		                              options:options
		                          deallocator:nil]
		: [mtlDevice newBufferWithLength:info->length options:options];

	if (!buffer)
		return NULL_OBJECT_HANDLE;

	info->memory.ptr = [buffer contents];
	info->gpu_address = [buffer gpuAddress];
	return Wrap(buffer);
}

obj_handle_t MTLDevice_newTexture(obj_handle_t device, struct WMTTextureInfo* info)
{
	if (!device || !info)
		return NULL_OBJECT_HANDLE;

	MTLTextureDescriptor* desc = DescribeTexture(*info);
	id<MTLTexture> texture = [Unwrap<id<MTLDevice>>(device) newTextureWithDescriptor:desc];
	[desc release];

	if (!texture)
		return NULL_OBJECT_HANDLE;

	/* The bindless handle the shaders index by; see d9mt's argument buffers. */
	info->gpu_resource_id = [texture gpuResourceID]._impl;
	return Wrap(texture);
}

obj_handle_t MTLBuffer_newTexture(obj_handle_t buffer, struct WMTTextureInfo* info,
                                  uint64_t offset, uint64_t bytes_per_row)
{
	if (!buffer || !info)
		return NULL_OBJECT_HANDLE;

	MTLTextureDescriptor* desc = DescribeTexture(*info);
	id<MTLTexture> texture = [Unwrap<id<MTLBuffer>>(buffer) newTextureWithDescriptor:desc
	                                                                          offset:offset
	                                                                     bytesPerRow:bytes_per_row];
	[desc release];

	if (!texture)
		return NULL_OBJECT_HANDLE;

	info->gpu_resource_id = [texture gpuResourceID]._impl;
	return Wrap(texture);
}

obj_handle_t MTLDevice_newSamplerState(obj_handle_t device, struct WMTSamplerInfo* info)
{
	if (!device || !info)
		return NULL_OBJECT_HANDLE;

	MTLSamplerDescriptor* desc = [[MTLSamplerDescriptor alloc] init];
	desc.minFilter = static_cast<MTLSamplerMinMagFilter>(info->min_filter);
	desc.magFilter = static_cast<MTLSamplerMinMagFilter>(info->mag_filter);
	desc.mipFilter = static_cast<MTLSamplerMipFilter>(info->mip_filter);
	desc.rAddressMode = static_cast<MTLSamplerAddressMode>(info->r_address_mode);
	desc.sAddressMode = static_cast<MTLSamplerAddressMode>(info->s_address_mode);
	desc.tAddressMode = static_cast<MTLSamplerAddressMode>(info->t_address_mode);
	desc.borderColor = static_cast<MTLSamplerBorderColor>(info->border_color);
	desc.compareFunction = static_cast<MTLCompareFunction>(info->compare_function);
	desc.lodMinClamp = info->lod_min_clamp;
	desc.lodMaxClamp = info->lod_max_clamp;
	desc.maxAnisotropy = info->max_anisotroy ? info->max_anisotroy : 1;
	desc.normalizedCoordinates = info->normalized_coords;
	desc.lodAverage = info->lod_average;
	/* Required before gpuResourceID can be read. */
	desc.supportArgumentBuffers = info->support_argument_buffers;

	id<MTLSamplerState> sampler = [Unwrap<id<MTLDevice>>(device) newSamplerStateWithDescriptor:desc];
	[desc release];

	if (!sampler)
		return NULL_OBJECT_HANDLE;

	if (info->support_argument_buffers)
		info->gpu_resource_id = [sampler gpuResourceID]._impl;
	return Wrap(sampler);
}

void MTLBuffer_didModifyRange(obj_handle_t buffer, uint64_t start, uint64_t length)
{
	if (!buffer)
		return;

	/*
	 * Only meaningful for Managed storage. On Apple silicon everything the
	 * engine allocates is Shared or Private, where this is a no-op -- but the
	 * backend calls it unconditionally, and calling it on a non-managed buffer
	 * is a Metal validation error, so the mode is checked.
	 */
	id<MTLBuffer> mtlBuffer = Unwrap<id<MTLBuffer>>(buffer);
	if ([mtlBuffer storageMode] == MTLStorageModeManaged)
		[mtlBuffer didModifyRange:NSMakeRange(start, length)];
}

void MTLBuffer_updateContents(obj_handle_t buffer, uint64_t offset,
                              struct WMTConstMemoryPointer data, uint64_t length)
{
	if (!buffer || !data.ptr)
		return;

	id<MTLBuffer> mtlBuffer = Unwrap<id<MTLBuffer>>(buffer);
	void* contents = [mtlBuffer contents];
	if (!contents)
		return;

	std::memcpy(static_cast<char*>(contents) + offset, data.ptr, length);

	if ([mtlBuffer storageMode] == MTLStorageModeManaged)
		[mtlBuffer didModifyRange:NSMakeRange(offset, length)];
}

obj_handle_t MTLTexture_newTextureView(obj_handle_t texture, uint32_t format, uint32_t texture_type,
                                       uint16_t level_start, uint16_t level_count,
                                       uint16_t slice_start, uint16_t slice_count,
                                       uint64_t* out_gpu_resource_id)
{
	if (!texture)
		return NULL_OBJECT_HANDLE;

	id<MTLTexture> view = [Unwrap<id<MTLTexture>>(texture)
		newTextureViewWithPixelFormat:static_cast<MTLPixelFormat>(format)
		                  textureType:static_cast<MTLTextureType>(texture_type)
		                       levels:NSMakeRange(level_start, level_count)
		                       slices:NSMakeRange(slice_start, slice_count)];

	if (!view)
		return NULL_OBJECT_HANDLE;

	if (out_gpu_resource_id)
		*out_gpu_resource_id = [view gpuResourceID]._impl;
	return Wrap(view);
}

void MTLTexture_replaceRegion(obj_handle_t texture, struct WMTOrigin origin, struct WMTSize size,
                              uint64_t level, uint64_t slice, struct WMTMemoryPointer data,
                              uint64_t bytes_per_row, uint64_t bytes_per_image)
{
	if (!texture || !data.ptr)
		return;

	const MTLRegion region = {
		{static_cast<NSUInteger>(origin.x), static_cast<NSUInteger>(origin.y), static_cast<NSUInteger>(origin.z)},
		{static_cast<NSUInteger>(size.width), static_cast<NSUInteger>(size.height), static_cast<NSUInteger>(size.depth)}
	};

	[Unwrap<id<MTLTexture>>(texture) replaceRegion:region
	                                   mipmapLevel:level
	                                         slice:slice
	                                     withBytes:data.ptr
	                                   bytesPerRow:bytes_per_row
	                                 bytesPerImage:bytes_per_image];
}

uint32_t MTLTexture_pixelFormat(obj_handle_t texture)
{
	return texture ? static_cast<uint32_t>([Unwrap<id<MTLTexture>>(texture) pixelFormat]) : 0;
}

uint64_t MTLTexture_width(obj_handle_t texture)
{
	return texture ? [Unwrap<id<MTLTexture>>(texture) width] : 0;
}

uint64_t MTLTexture_height(obj_handle_t texture)
{
	return texture ? [Unwrap<id<MTLTexture>>(texture) height] : 0;
}

uint64_t MTLTexture_depth(obj_handle_t texture)
{
	return texture ? [Unwrap<id<MTLTexture>>(texture) depth] : 0;
}

uint64_t MTLTexture_arrayLength(obj_handle_t texture)
{
	return texture ? [Unwrap<id<MTLTexture>>(texture) arrayLength] : 0;
}

uint64_t MTLTexture_mipmapLevelCount(obj_handle_t texture)
{
	return texture ? [Unwrap<id<MTLTexture>>(texture) mipmapLevelCount] : 0;
}

uint64_t MTLDevice_minimumLinearTextureAlignmentForPixelFormat(obj_handle_t device, uint32_t format)
{
	if (!device)
		return 0;

	return [Unwrap<id<MTLDevice>>(device)
		minimumLinearTextureAlignmentForPixelFormat:static_cast<MTLPixelFormat>(format)];
}
