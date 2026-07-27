/*
 * Libraries, pipeline state, encoders and synchronisation.
 *
 * Together with metalbridge_commands.mm this is the whole frame path: create a
 * pipeline, open an encoder, replay a command list onto it, present.
 */

#include "metalbridge_pipeline.h"

#import <Metal/Metal.h>

#include <cstring>

namespace
{

template <class _Obj> inline _Obj Unwrap(obj_handle_t handle)
{
	return (__bridge _Obj)(void*)handle;
}

inline obj_handle_t Wrap(id obj)
{
	return (obj_handle_t)(__bridge void*)obj;
}

void ApplyStencil(MTLStencilDescriptor* dst, const WMTStencilInfo& src)
{
	dst.stencilCompareFunction = static_cast<MTLCompareFunction>(src.stencil_compare_function);
	dst.stencilFailureOperation = static_cast<MTLStencilOperation>(src.stencil_fail_op);
	dst.depthFailureOperation = static_cast<MTLStencilOperation>(src.depth_fail_op);
	dst.depthStencilPassOperation = static_cast<MTLStencilOperation>(src.depth_stencil_pass_op);
	dst.writeMask = src.write_mask;
	dst.readMask = src.read_mask;
}

}

obj_handle_t MTLDevice_newLibrary(obj_handle_t device, const void* bytecode, uint64_t length)
{
	if (!device || !bytecode || !length)
		return NULL_OBJECT_HANDLE;

	/*
	 * A metallib blob, not source: d9mt compiles SPIR-V to MSL and then to a
	 * metallib before it gets here. dispatch_data_create with a null destructor
	 * borrows the bytes, and newLibraryWithData copies what it keeps.
	 */
	dispatch_data_t data = dispatch_data_create(bytecode, length, nullptr,
		DISPATCH_DATA_DESTRUCTOR_DEFAULT);

	NSError* error = nil;
	id<MTLLibrary> library = [Unwrap<id<MTLDevice>>(device) newLibraryWithData:data error:&error];
	dispatch_release(data);

	return Wrap(library);
}

obj_handle_t MTLLibrary_newFunction(obj_handle_t library, const char* name)
{
	if (!library || !name)
		return NULL_OBJECT_HANDLE;

	return Wrap([Unwrap<id<MTLLibrary>>(library) newFunctionWithName:@(name)]);
}

obj_handle_t MTLDevice_newRenderPipelineState(obj_handle_t device,
                                              const struct WMTRenderPipelineInfo* info,
                                              obj_handle_t* out_error)
{
	if (out_error)
		*out_error = NULL_OBJECT_HANDLE;
	if (!device || !info)
		return NULL_OBJECT_HANDLE;

	MTLRenderPipelineDescriptor* desc = [[MTLRenderPipelineDescriptor alloc] init];
	desc.vertexFunction = Unwrap<id<MTLFunction>>(info->vertex_function);
	desc.fragmentFunction = Unwrap<id<MTLFunction>>(info->fragment_function);
	desc.alphaToCoverageEnabled = info->alpha_to_coverage_enabled;
	desc.rasterizationEnabled = info->rasterization_enabled;
	desc.rasterSampleCount = info->raster_sample_count ? info->raster_sample_count : 1;
	desc.depthAttachmentPixelFormat = static_cast<MTLPixelFormat>(info->depth_pixel_format);
	desc.stencilAttachmentPixelFormat = static_cast<MTLPixelFormat>(info->stencil_pixel_format);
	desc.inputPrimitiveTopology =
		static_cast<MTLPrimitiveTopologyClass>(info->input_primitive_topology);

	for (int i = 0; i < 8; ++i)
	{
		const WMTColorAttachmentBlendInfo& src = info->colors[i];
		MTLRenderPipelineColorAttachmentDescriptor* dst = desc.colorAttachments[i];

		dst.pixelFormat = static_cast<MTLPixelFormat>(src.pixel_format);
		dst.blendingEnabled = src.blending_enabled;
		dst.sourceRGBBlendFactor = static_cast<MTLBlendFactor>(src.src_rgb_blend_factor);
		dst.destinationRGBBlendFactor = static_cast<MTLBlendFactor>(src.dst_rgb_blend_factor);
		dst.rgbBlendOperation = static_cast<MTLBlendOperation>(src.rgb_blend_operation);
		dst.sourceAlphaBlendFactor = static_cast<MTLBlendFactor>(src.src_alpha_blend_factor);
		dst.destinationAlphaBlendFactor = static_cast<MTLBlendFactor>(src.dst_alpha_blend_factor);
		dst.alphaBlendOperation = static_cast<MTLBlendOperation>(src.alpha_blend_operation);
		dst.writeMask = static_cast<MTLColorWriteMask>(src.write_mask);
	}

	NSError* error = nil;
	id<MTLRenderPipelineState> pso =
		[Unwrap<id<MTLDevice>>(device) newRenderPipelineStateWithDescriptor:desc error:&error];
	[desc release];

	/* The backend logs this; it survives because NSError is autoreleased into the pool. */
	if (!pso && out_error && error)
		*out_error = Wrap(error);

	return Wrap(pso);
}

obj_handle_t MTLDevice_newComputePipelineState(obj_handle_t device,
                                               const struct WMTComputePipelineInfo* info,
                                               obj_handle_t* out_error)
{
	if (out_error)
		*out_error = NULL_OBJECT_HANDLE;
	if (!device || !info)
		return NULL_OBJECT_HANDLE;

	MTLComputePipelineDescriptor* desc = [[MTLComputePipelineDescriptor alloc] init];
	desc.computeFunction = Unwrap<id<MTLFunction>>(info->compute_function);
	desc.threadGroupSizeIsMultipleOfThreadExecutionWidth = info->tgsize_is_multiple_of_sgwidth;

	NSError* error = nil;
	id<MTLComputePipelineState> pso =
		[Unwrap<id<MTLDevice>>(device) newComputePipelineStateWithDescriptor:desc
		                                                            options:MTLPipelineOptionNone
		                                                         reflection:nil
		                                                              error:&error];
	[desc release];

	if (!pso && out_error && error)
		*out_error = Wrap(error);

	return Wrap(pso);
}

obj_handle_t MTLDevice_newDepthStencilState(obj_handle_t device, const struct WMTDepthStencilInfo* info)
{
	if (!device || !info)
		return NULL_OBJECT_HANDLE;

	MTLDepthStencilDescriptor* desc = [[MTLDepthStencilDescriptor alloc] init];
	desc.depthCompareFunction = static_cast<MTLCompareFunction>(info->depth_compare_function);
	desc.depthWriteEnabled = info->depth_write_enabled;
	ApplyStencil(desc.frontFaceStencil, info->front_stencil);
	ApplyStencil(desc.backFaceStencil, info->back_stencil);

	id<MTLDepthStencilState> dsso = [Unwrap<id<MTLDevice>>(device) newDepthStencilStateWithDescriptor:desc];
	[desc release];
	return Wrap(dsso);
}

obj_handle_t MTLCommandBuffer_renderCommandEncoder(obj_handle_t cmdbuf, const struct WMTRenderPassInfo* info)
{
	if (!cmdbuf || !info)
		return NULL_OBJECT_HANDLE;

	MTLRenderPassDescriptor* pass = [MTLRenderPassDescriptor renderPassDescriptor];

	for (int i = 0; i < 8; ++i)
	{
		const WMTColorAttachmentInfo& src = info->colors[i];
		if (!src.texture)
			continue;

		MTLRenderPassColorAttachmentDescriptor* dst = pass.colorAttachments[i];
		dst.texture = Unwrap<id<MTLTexture>>(src.texture);
		dst.level = src.level;
		dst.slice = src.slice;
		dst.depthPlane = src.depth_plane;
		dst.loadAction = static_cast<MTLLoadAction>(src.load_action);
		dst.storeAction = static_cast<MTLStoreAction>(src.store_action);
		dst.clearColor = MTLClearColorMake(src.clear_color.r, src.clear_color.g,
		                                   src.clear_color.b, src.clear_color.a);
		if (src.resolve_texture)
			dst.resolveTexture = Unwrap<id<MTLTexture>>(src.resolve_texture);
	}

	if (info->depth.texture)
	{
		pass.depthAttachment.texture = Unwrap<id<MTLTexture>>(info->depth.texture);
		pass.depthAttachment.level = info->depth.level;
		pass.depthAttachment.slice = info->depth.slice;
		pass.depthAttachment.depthPlane = info->depth.depth_plane;
		pass.depthAttachment.loadAction = static_cast<MTLLoadAction>(info->depth.load_action);
		pass.depthAttachment.storeAction = static_cast<MTLStoreAction>(info->depth.store_action);
		pass.depthAttachment.clearDepth = info->depth.clear_depth;
	}

	if (info->stencil.texture)
	{
		pass.stencilAttachment.texture = Unwrap<id<MTLTexture>>(info->stencil.texture);
		pass.stencilAttachment.level = info->stencil.level;
		pass.stencilAttachment.slice = info->stencil.slice;
		pass.stencilAttachment.depthPlane = info->stencil.depth_plane;
		pass.stencilAttachment.loadAction = static_cast<MTLLoadAction>(info->stencil.load_action);
		pass.stencilAttachment.storeAction = static_cast<MTLStoreAction>(info->stencil.store_action);
		pass.stencilAttachment.clearStencil = info->stencil.clear_stencil;
	}

	pass.renderTargetWidth = info->render_target_width;
	pass.renderTargetHeight = info->render_target_height;
	pass.renderTargetArrayLength = info->render_target_array_length;
	pass.defaultRasterSampleCount = info->default_raster_sample_count;
	pass.tileWidth = info->tile_width;
	pass.tileHeight = info->tile_height;

	if (info->visibility_buffer)
		pass.visibilityResultBuffer = Unwrap<id<MTLBuffer>>(info->visibility_buffer);

	/* Retained: Metal's encoders are autoreleased, the ABI hands over ownership. */
	return Wrap([[Unwrap<id<MTLCommandBuffer>>(cmdbuf) renderCommandEncoderWithDescriptor:pass] retain]);
}

obj_handle_t MTLCommandBuffer_computeCommandEncoder(obj_handle_t cmdbuf, bool concurrent)
{
	if (!cmdbuf)
		return NULL_OBJECT_HANDLE;

	const MTLDispatchType type = concurrent ? MTLDispatchTypeConcurrent : MTLDispatchTypeSerial;
	return Wrap([[Unwrap<id<MTLCommandBuffer>>(cmdbuf) computeCommandEncoderWithDispatchType:type] retain]);
}

obj_handle_t MTLCommandBuffer_blitCommandEncoder(obj_handle_t cmdbuf)
{
	if (!cmdbuf)
		return NULL_OBJECT_HANDLE;

	return Wrap([[Unwrap<id<MTLCommandBuffer>>(cmdbuf) blitCommandEncoder] retain]);
}

obj_handle_t MTLDevice_newFence(obj_handle_t device)
{
	return device ? Wrap([Unwrap<id<MTLDevice>>(device) newFence]) : NULL_OBJECT_HANDLE;
}

obj_handle_t MTLDevice_newEvent(obj_handle_t device)
{
	return device ? Wrap([Unwrap<id<MTLDevice>>(device) newEvent]) : NULL_OBJECT_HANDLE;
}

obj_handle_t MTLDevice_newSharedEvent(obj_handle_t device)
{
	return device ? Wrap([Unwrap<id<MTLDevice>>(device) newSharedEvent]) : NULL_OBJECT_HANDLE;
}

void MTLCommandBuffer_encodeSignalEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value)
{
	if (cmdbuf && event)
		[Unwrap<id<MTLCommandBuffer>>(cmdbuf) encodeSignalEvent:Unwrap<id<MTLEvent>>(event) value:value];
}

void MTLCommandBuffer_encodeWaitForEvent(obj_handle_t cmdbuf, obj_handle_t event, uint64_t value)
{
	if (cmdbuf && event)
		[Unwrap<id<MTLCommandBuffer>>(cmdbuf) encodeWaitForEvent:Unwrap<id<MTLEvent>>(event) value:value];
}

uint64_t MTLSharedEvent_signaledValue(obj_handle_t event)
{
	return event ? [Unwrap<id<MTLSharedEvent>>(event) signaledValue] : 0;
}

void MTLSharedEvent_signalValue(obj_handle_t event, uint64_t value)
{
	if (event)
		[Unwrap<id<MTLSharedEvent>>(event) setSignaledValue:value];
}

bool MTLSharedEvent_waitUntilSignaledValue(obj_handle_t event, uint64_t value, uint64_t timeout_ms)
{
	if (!event)
		return false;

	return [Unwrap<id<MTLSharedEvent>>(event) waitUntilSignaledValue:value timeoutMS:timeout_ms];
}

uint64_t MTLDevice_currentAllocatedSize(obj_handle_t device)
{
	return device ? [Unwrap<id<MTLDevice>>(device) currentAllocatedSize] : 0;
}

uint64_t MTLDevice_registryID(obj_handle_t device)
{
	return device ? [Unwrap<id<MTLDevice>>(device) registryID] : 0;
}

bool MTLDevice_supportsTextureSampleCount(obj_handle_t device, uint64_t count)
{
	return device && [Unwrap<id<MTLDevice>>(device) supportsTextureSampleCount:count];
}

bool MTLDevice_supportsBCTextureCompression(obj_handle_t device)
{
	return device && [Unwrap<id<MTLDevice>>(device) supportsBCTextureCompression];
}

void MTLDevice_setShouldMaximizeConcurrentCompilation(obj_handle_t device, bool value)
{
	if (device)
		[Unwrap<id<MTLDevice>>(device) setShouldMaximizeConcurrentCompilation:value];
}

void MTLCommandBuffer_presentDrawable(obj_handle_t cmdbuf, obj_handle_t drawable)
{
	if (cmdbuf && drawable)
		[Unwrap<id<MTLCommandBuffer>>(cmdbuf) presentDrawable:Unwrap<id<MTLDrawable>>(drawable)];
}

/* ---- functions with specialisation constants ---- */

obj_handle_t MTLLibrary_newFunctionWithConstants(obj_handle_t library, const char* name,
	const struct WMTFunctionConstant* constants, uint32_t num_constants, obj_handle_t* err_out)
{
	if (err_out)
		*err_out = NULL_OBJECT_HANDLE;
	if (!library || !name)
		return NULL_OBJECT_HANDLE;

	MTLFunctionConstantValues* values = [[MTLFunctionConstantValues alloc] init];
	for (uint32_t i = 0; i < num_constants; ++i)
	{
		[values setConstantValue:constants[i].data.ptr
		                    type:static_cast<MTLDataType>(constants[i].type)
		                 atIndex:constants[i].index];
	}

	NSError* error = nil;
	id<MTLFunction> fn = [Unwrap<id<MTLLibrary>>(library) newFunctionWithName:@(name)
	                                                          constantValues:values
	                                                                   error:&error];
	[values release];

	if (!fn && err_out && error)
		*err_out = Wrap(error);

	return Wrap(fn);
}

obj_handle_t MTLCommandBuffer_error(obj_handle_t cmdbuf)
{
	return cmdbuf ? Wrap([Unwrap<id<MTLCommandBuffer>>(cmdbuf) error]) : NULL_OBJECT_HANDLE;
}

uint64_t MTLCommandBuffer_property(obj_handle_t cmdbuf, enum WMTCommandBufferProperty prop)
{
	if (!cmdbuf)
		return 0;

	id<MTLCommandBuffer> cb = Unwrap<id<MTLCommandBuffer>>(cmdbuf);

	/* Metal reports these as seconds; the ABI carries nanoseconds. */
	CFTimeInterval t = 0.0;
	switch (prop)
	{
	case WMTCommandBufferPropertyKernelStartTime: t = [cb kernelStartTime]; break;
	case WMTCommandBufferPropertyKernelEndTime:   t = [cb kernelEndTime];   break;
	case WMTCommandBufferPropertyGPUStartTime:    t = [cb GPUStartTime];    break;
	case WMTCommandBufferPropertyGPUEndTime:      t = [cb GPUEndTime];      break;
	}
	return static_cast<uint64_t>(t * 1e9);
}

obj_handle_t NSAutoreleasePool_alloc_init(void)
{
	return Wrap([[NSAutoreleasePool alloc] init]);
}

obj_handle_t NSObject_description(obj_handle_t obj)
{
	return obj ? Wrap([Unwrap<id>(obj) description]) : NULL_OBJECT_HANDLE;
}

uint64_t NSString_getCString(obj_handle_t str, char* buffer, uint64_t maxLength, uint32_t encoding)
{
	if (!str || !buffer || maxLength == 0)
		return 0;

	NSString* s = Unwrap<NSString*>(str);
	const BOOL ok = [s getCString:buffer maxLength:maxLength
	                     encoding:static_cast<NSStringEncoding>(encoding)];
	return ok ? std::strlen(buffer) : 0;
}
