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
