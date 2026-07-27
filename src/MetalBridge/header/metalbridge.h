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

/* Bytes of the device name, UTF-8, truncated to `size`. Returns bytes needed. */
uint64_t MTLDevice_name(obj_handle_t device, char* buffer, uint64_t size);

uint64_t MTLDevice_recommendedMaxWorkingSetSize(obj_handle_t device);
uint8_t  MTLDevice_hasUnifiedMemory(obj_handle_t device);
uint8_t  MTLDevice_supportsFamily(obj_handle_t device, uint32_t family);

/* ---- command queue and buffer ---- */

obj_handle_t MTLCommandQueue_commandBuffer(obj_handle_t queue);

void MTLCommandBuffer_commit(obj_handle_t cmdbuf);
void MTLCommandBuffer_waitUntilCompleted(obj_handle_t cmdbuf);
uint32_t MTLCommandBuffer_status(obj_handle_t cmdbuf);

#ifdef __cplusplus
}
#endif

#endif /* METALBRIDGE_H */
