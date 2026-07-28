/*
 * The command-stream interpreter: replays a batched winemetal command list onto
 * a Metal encoder.
 *
 * d9mt's backend builds a linked list of wmtcmd_* structs and hands over the
 * head; each encodeCommands walks it and issues the Metal calls. The batching
 * exists to amortise Wine boundary crossings, which this build does not have --
 * the format is honoured so the backend links unmodified, not because it earns
 * anything here.
 *
 * Every WMT enum carries Metal's own numeric value, so the conversions are
 * casts. See metalbridge.mm.
 */

#include "metalbridge_commands.h"

#import <Metal/Metal.h>

#include <cstdio>

namespace
{

template <class _Obj> inline _Obj Unwrap(obj_handle_t handle)
{
	return (__bridge _Obj)(void*)handle;
}

inline const wmtcmd_base* Next(const wmtcmd_base* cmd)
{
	return static_cast<const wmtcmd_base*>(cmd->next.ptr);
}

/* Casting the command to its payload type is the whole dispatch. */
template <class _Cmd> inline const _Cmd& As(const wmtcmd_base* cmd)
{
	return *reinterpret_cast<const _Cmd*>(cmd);
}

inline MTLSize ToMTLSize(const WMTSize& s)
{
	return MTLSizeMake(s.width, s.height, s.depth);
}

inline MTLOrigin ToMTLOrigin(const WMTOrigin& o)
{
	return MTLOriginMake(o.x, o.y, o.z);
}

/*
 * Reported once per command type. A silently ignored command shows up much
 * later as a missing draw or a corrupt frame; this names it at the point it
 * happens.
 */
void Unsupported(const char* encoder, unsigned type)
{
	static bool seen[256] = {};
	if (type < 256 && seen[type])
		return;
	if (type < 256)
		seen[type] = true;

	std::fprintf(stderr, "MetalBridge: unhandled %s command %u\n", encoder, type);
}

}

void MTLCommandEncoder_endEncoding(obj_handle_t encoder)
{
	if (encoder)
		[Unwrap<id<MTLCommandEncoder>>(encoder) endEncoding];
}

void MTLCommandEncoder_setLabel(obj_handle_t encoder, const char* label)
{
	if (encoder && label)
		[Unwrap<id<MTLCommandEncoder>>(encoder) setLabel:@(label)];
}

void MTLBlitCommandEncoder_encodeCommands(obj_handle_t encoder, const struct wmtcmd_base* cmd_head)
{
	if (!encoder)
		return;

	id<MTLBlitCommandEncoder> enc = Unwrap<id<MTLBlitCommandEncoder>>(encoder);

	for (const wmtcmd_base* cmd = cmd_head; cmd; cmd = Next(cmd))
	{
		switch (static_cast<WMTBlitCommandType>(cmd->type))
		{
		case WMTBlitCommandNop:
			break;

		case WMTBlitCommandCopyFromBufferToBuffer:
		{
			const auto& c = As<wmtcmd_blit_copy_from_buffer_to_buffer>(cmd);
			[enc copyFromBuffer:Unwrap<id<MTLBuffer>>(c.src)
			       sourceOffset:c.src_offset
			           toBuffer:Unwrap<id<MTLBuffer>>(c.dst)
			  destinationOffset:c.dst_offset
			               size:c.copy_length];
			break;
		}

		case WMTBlitCommandCopyFromBufferToTexture:
		{
			const auto& c = As<wmtcmd_blit_copy_from_buffer_to_texture>(cmd);

			/* The alpha actually reaching the GPU -- this is how textures upload.
			 *
			 * Select on the destination's pixel format, not on the source pitch.
			 * A DXT3/DXT5 row is 16 bytes per 4x4 block, which is exactly
			 * width * 4 -- so a pitch test admits compressed uploads and counts
			 * block bits as if they were alpha. The first cut did that, and its
			 * log filled with DDS uploads before a single GUI texture appeared. */
			if (::rrr3d::TraceEnabled() && c.src && c.dst && c.level == 0)
			{
				id<MTLTexture> t = Unwrap<id<MTLTexture>>(c.dst);
				const MTLPixelFormat pf = [t pixelFormat];
				const bool rgba8 = pf == MTLPixelFormatBGRA8Unorm || pf == MTLPixelFormatRGBA8Unorm ||
				                   pf == MTLPixelFormatBGRA8Unorm_sRGB || pf == MTLPixelFormatRGBA8Unorm_sRGB;

				static int logged = 0;
				if (rgba8 && c.bytes_per_row >= c.size.width * 4 && logged < 64)
				{
					++logged;
					id<MTLBuffer> b = Unwrap<id<MTLBuffer>>(c.src);
					const unsigned char* px =
						(const unsigned char*)[b contents] + c.src_offset;
					unsigned translucent = 0;
					for (uint64_t y = 0; y < c.size.height; ++y)
					{
						const unsigned char* row = px + y * c.bytes_per_row;
						for (uint64_t x = 0; x < c.size.width; ++x)
							if (row[x * 4 + 3] != 255)
								++translucent;
					}
					RRR3D_TRACE("BLITTEX %llux%llu level=%llu pf=%u pitch=%llu translucent=%u/%llu",
						(unsigned long long)c.size.width, (unsigned long long)c.size.height,
						(unsigned long long)c.level, (unsigned)pf,
						(unsigned long long)c.bytes_per_row, translucent,
						(unsigned long long)(c.size.width * c.size.height));
				}
			}
			[enc copyFromBuffer:Unwrap<id<MTLBuffer>>(c.src)
			       sourceOffset:c.src_offset
			  sourceBytesPerRow:c.bytes_per_row
			sourceBytesPerImage:c.bytes_per_image
			         sourceSize:ToMTLSize(c.size)
			          toTexture:Unwrap<id<MTLTexture>>(c.dst)
			   destinationSlice:c.slice
			   destinationLevel:c.level
			  destinationOrigin:ToMTLOrigin(c.origin)];
			break;
		}

		case WMTBlitCommandCopyFromBufferToTextureWithBlitOption:
		{
			const auto& c = As<wmtcmd_blit_copy_from_buffer_to_texture_withblitoption>(cmd);
			[enc copyFromBuffer:Unwrap<id<MTLBuffer>>(c.src)
			       sourceOffset:c.src_offset
			  sourceBytesPerRow:c.bytes_per_row
			sourceBytesPerImage:c.bytes_per_image
			         sourceSize:ToMTLSize(c.size)
			          toTexture:Unwrap<id<MTLTexture>>(c.dst)
			   destinationSlice:c.slice
			   destinationLevel:c.level
			  destinationOrigin:ToMTLOrigin(c.origin)
			            options:static_cast<MTLBlitOption>(c.options)];
			break;
		}

		case WMTBlitCommandCopyFromTextureToTexture:
		{
			const auto& c = As<wmtcmd_blit_copy_from_texture_to_texture>(cmd);
			RRR3D_TRACE_FIRST(12, "BLIT tex %p -> %p size=%llux%llu",
				(void*)(uintptr_t)c.src, (void*)(uintptr_t)c.dst,
				(unsigned long long)c.src_size.width,
				(unsigned long long)c.src_size.height);
			[enc copyFromTexture:Unwrap<id<MTLTexture>>(c.src)
			         sourceSlice:c.src_slice
			         sourceLevel:c.src_level
			        sourceOrigin:ToMTLOrigin(c.src_origin)
			          sourceSize:ToMTLSize(c.src_size)
			           toTexture:Unwrap<id<MTLTexture>>(c.dst)
			    destinationSlice:c.dst_slice
			    destinationLevel:c.dst_level
			   destinationOrigin:ToMTLOrigin(c.dst_origin)];
			break;
		}

		case WMTBlitCommandCopyFromTextureToBuffer:
		{
			const auto& c = As<wmtcmd_blit_copy_from_texture_to_buffer>(cmd);
			[enc copyFromTexture:Unwrap<id<MTLTexture>>(c.src)
			         sourceSlice:c.slice
			         sourceLevel:c.level
			        sourceOrigin:ToMTLOrigin(c.origin)
			          sourceSize:ToMTLSize(c.size)
			            toBuffer:Unwrap<id<MTLBuffer>>(c.dst)
			   destinationOffset:c.offset
			destinationBytesPerRow:c.bytes_per_row
			destinationBytesPerImage:c.bytes_per_image];
			break;
		}

		case WMTBlitCommandGenerateMipmaps:
		{
			const auto& c = As<wmtcmd_blit_generate_mipmaps>(cmd);
			[enc generateMipmapsForTexture:Unwrap<id<MTLTexture>>(c.texture)];
			break;
		}

		case WMTBlitCommandFillBuffer:
		{
			const auto& c = As<wmtcmd_blit_fillbuffer>(cmd);
			[enc fillBuffer:Unwrap<id<MTLBuffer>>(c.buffer)
			          range:NSMakeRange(c.offset, c.length)
			          value:c.value];
			break;
		}

		case WMTBlitCommandWaitForFence:
		{
			const auto& c = As<wmtcmd_blit_fence_op>(cmd);
			[enc waitForFence:Unwrap<id<MTLFence>>(c.fence)];
			break;
		}

		case WMTBlitCommandUpdateFence:
		{
			const auto& c = As<wmtcmd_blit_fence_op>(cmd);
			[enc updateFence:Unwrap<id<MTLFence>>(c.fence)];
			break;
		}

		case WMTBlitCommandResolveCounters:
		{
			const auto& c = As<wmtcmd_blit_resolvecounters>(cmd);
			[enc resolveCounters:Unwrap<id<MTLCounterSampleBuffer>>(c.sample_buffer)
			             inRange:NSMakeRange(c.start, c.len)
			   destinationBuffer:Unwrap<id<MTLBuffer>>(c.dst_buffer)
			   destinationOffset:c.dst_offset];
			break;
		}

		default:
			Unsupported("blit", cmd->type);
			break;
		}
	}
}

void MTLComputeCommandEncoder_encodeCommands(obj_handle_t encoder, const struct wmtcmd_base* cmd_head)
{
	if (!encoder)
		return;

	id<MTLComputeCommandEncoder> enc = Unwrap<id<MTLComputeCommandEncoder>>(encoder);
	MTLSize threadgroupSize = MTLSizeMake(1, 1, 1);

	for (const wmtcmd_base* cmd = cmd_head; cmd; cmd = Next(cmd))
	{
		switch (static_cast<WMTComputeCommandType>(cmd->type))
		{
		case WMTComputeCommandNop:
			break;

		case WMTComputeCommandSetPSO:
		{
			const auto& c = As<wmtcmd_compute_setpso>(cmd);
			[enc setComputePipelineState:Unwrap<id<MTLComputePipelineState>>(c.pso)];
			/* Carried on the command because Metal takes it per dispatch. */
			threadgroupSize = ToMTLSize(c.threadgroup_size);
			break;
		}

		case WMTComputeCommandSetBuffer:
		{
			const auto& c = As<wmtcmd_compute_setbuffer>(cmd);
			[enc setBuffer:Unwrap<id<MTLBuffer>>(c.buffer) offset:c.offset atIndex:c.index];
			break;
		}

		case WMTComputeCommandSetBufferOffset:
		{
			const auto& c = As<wmtcmd_compute_setbufferoffset>(cmd);
			[enc setBufferOffset:c.offset atIndex:c.index];
			break;
		}

		case WMTComputeCommandSetBytes:
		{
			const auto& c = As<wmtcmd_compute_setbytes>(cmd);
			[enc setBytes:c.bytes.ptr length:c.length atIndex:c.index];
			break;
		}

		case WMTComputeCommandSetTexture:
		{
			const auto& c = As<wmtcmd_compute_settexture>(cmd);
			[enc setTexture:Unwrap<id<MTLTexture>>(c.texture) atIndex:c.index];
			break;
		}

		case WMTComputeCommandUseResource:
		{
			const auto& c = As<wmtcmd_compute_useresource>(cmd);
			[enc useResource:Unwrap<id<MTLResource>>(c.resource)
			           usage:static_cast<MTLResourceUsage>(c.usage)];
			break;
		}

		case WMTComputeCommandDispatch:
		{
			const auto& c = As<wmtcmd_compute_dispatch>(cmd);
			[enc dispatchThreadgroups:ToMTLSize(c.size) threadsPerThreadgroup:threadgroupSize];
			break;
		}

		case WMTComputeCommandDispatchThreads:
		{
			const auto& c = As<wmtcmd_compute_dispatch>(cmd);
			[enc dispatchThreads:ToMTLSize(c.size) threadsPerThreadgroup:threadgroupSize];
			break;
		}

		case WMTComputeCommandDispatchIndirect:
		{
			const auto& c = As<wmtcmd_compute_dispatch_indirect>(cmd);
			[enc dispatchThreadgroupsWithIndirectBuffer:Unwrap<id<MTLBuffer>>(c.indirect_args_buffer)
			                       indirectBufferOffset:c.indirect_args_offset
			                      threadsPerThreadgroup:threadgroupSize];
			break;
		}

		case WMTComputeCommandWaitForFence:
		{
			const auto& c = As<wmtcmd_compute_fence_op>(cmd);
			[enc waitForFence:Unwrap<id<MTLFence>>(c.fence)];
			break;
		}

		case WMTComputeCommandUpdateFence:
		{
			const auto& c = As<wmtcmd_compute_fence_op>(cmd);
			[enc updateFence:Unwrap<id<MTLFence>>(c.fence)];
			break;
		}

		case WMTComputeCommandMemoryBarrier:
		{
			const auto& c = As<wmtcmd_compute_memory_barrier>(cmd);
			[enc memoryBarrierWithScope:static_cast<MTLBarrierScope>(c.scope)];
			break;
		}

		default:
			Unsupported("compute", cmd->type);
			break;
		}
	}
}

void MTLRenderCommandEncoder_encodeCommands(obj_handle_t encoder, const struct wmtcmd_base* cmd_head)
{
	if (!encoder)
		return;

	id<MTLRenderCommandEncoder> enc = Unwrap<id<MTLRenderCommandEncoder>>(encoder);

	/*
	 * The command sequence handed to one encoder.
	 *
	 * This is the trace that found the stencil bug: diffing an encoder that
	 * drew against one that did not showed they differed by a single command.
	 * Command types are numeric because the enum is the ABI's, and the numbers
	 * are what you match against winemetal.h.
	 */
	if (::rrr3d::TraceEnabled())
	{
		static int logged = 0;
		if (logged < 8)
		{
			++logged;
			char seq[512];
			size_t used = 0;
			for (const wmtcmd_base* c = cmd_head; c && used + 8 < sizeof(seq); c = Next(c))
				used += std::snprintf(seq + used, sizeof(seq) - used, " %u", (unsigned)c->type);
			seq[used] = '\0';
			RRR3D_TRACE("SEQ enc=%llx :%s", (unsigned long long)encoder, seq);
		}
	}

	for (const wmtcmd_base* cmd = cmd_head; cmd; cmd = Next(cmd))
	{
		if (cmd->type == WMTRenderCommandDraw)
			RRR3D_TRACE_FIRST(60, "  draw on enc=%llx", (unsigned long long)encoder);

		switch (static_cast<WMTRenderCommandType>(cmd->type))
		{
		case WMTRenderCommandNop:
			break;

		/* ---- binding ----
		 *
		 * The ABI folds all four shader stages onto one struct and distinguishes
		 * them by command type, so these share a payload and differ only in
		 * which setter they reach.
		 */
		case WMTRenderCommandSetVertexBuffer:
		{
			const auto& c = As<wmtcmd_render_setbuffer>(cmd);
			/*
			 * Argument buffer contents, as the 64-bit GPU addresses they hold.
			 * Read them as anything else and a valid address looks like zero --
			 * printed as floats these are denormals, which cost an hour once.
			 */
			if (c.index == 0 && c.buffer && ::rrr3d::TraceEnabled())
			{
				static int seen = 0;
				static int logged = 0;
				if (++seen > 300 && logged < 4)
				{
					++logged;
					id<MTLBuffer> b = Unwrap<id<MTLBuffer>>(c.buffer);
					const uint64_t* q = (const uint64_t*)((const char*)[b contents] + c.offset);
					RRR3D_TRACE("AB@draw off=%llu : 0x%llx 0x%llx 0x%llx 0x%llx",
						(unsigned long long)c.offset,
						(unsigned long long)q[0], (unsigned long long)q[1],
						(unsigned long long)q[2], (unsigned long long)q[3]);
				}
			}
			[enc setVertexBuffer:Unwrap<id<MTLBuffer>>(c.buffer) offset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetVertexBufferOffset:
		{
			const auto& c = As<wmtcmd_render_setbufferoffset>(cmd);
			[enc setVertexBufferOffset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetFragmentBuffer:
		{
			const auto& c = As<wmtcmd_render_setbuffer>(cmd);
			[enc setFragmentBuffer:Unwrap<id<MTLBuffer>>(c.buffer) offset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetFragmentBufferOffset:
		{
			const auto& c = As<wmtcmd_render_setbufferoffset>(cmd);
			[enc setFragmentBufferOffset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetMeshBuffer:
		{
			const auto& c = As<wmtcmd_render_setbuffer>(cmd);
			[enc setMeshBuffer:Unwrap<id<MTLBuffer>>(c.buffer) offset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetMeshBufferOffset:
		{
			const auto& c = As<wmtcmd_render_setbufferoffset>(cmd);
			[enc setMeshBufferOffset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetObjectBuffer:
		{
			const auto& c = As<wmtcmd_render_setbuffer>(cmd);
			[enc setObjectBuffer:Unwrap<id<MTLBuffer>>(c.buffer) offset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetObjectBufferOffset:
		{
			const auto& c = As<wmtcmd_render_setbufferoffset>(cmd);
			[enc setObjectBufferOffset:c.offset atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetFragmentTexture:
		{
			const auto& c = As<wmtcmd_render_settexture>(cmd);
			[enc setFragmentTexture:Unwrap<id<MTLTexture>>(c.texture) atIndex:c.index];
			break;
		}
		case WMTRenderCommandSetFragmentBytes:
		{
			const auto& c = As<wmtcmd_render_setbytes>(cmd);
			[enc setFragmentBytes:c.bytes.ptr length:c.length atIndex:c.index];
			break;
		}

		case WMTRenderCommandUseResource:
		{
			const auto& c = As<wmtcmd_render_useresource>(cmd);
			[enc useResource:Unwrap<id<MTLResource>>(c.resource)
			           usage:static_cast<MTLResourceUsage>(c.usage)
			          stages:static_cast<MTLRenderStages>(c.stages)];
			break;
		}

		/* ---- state ---- */
		case WMTRenderCommandSetPSO:
		{
			const auto& c = As<wmtcmd_render_setpso>(cmd);
			[enc setRenderPipelineState:Unwrap<id<MTLRenderPipelineState>>(c.pso)];
			break;
		}

		case WMTRenderCommandSetDSSO:
		{
			const auto& c = As<wmtcmd_render_setdsso>(cmd);
			[enc setDepthStencilState:Unwrap<id<MTLDepthStencilState>>(c.dsso)];
			[enc setStencilReferenceValue:c.stencil_ref];
			break;
		}

		case WMTRenderCommandSetBlendFactorAndStencilRef:
		{
			const auto& c = As<wmtcmd_render_setblendcolor>(cmd);
			[enc setBlendColorRed:c.red green:c.green blue:c.blue alpha:c.alpha];
			[enc setStencilReferenceValue:c.stencil_ref];
			break;
		}

		case WMTRenderCommandSetRasterizerState:
		{
			const auto& c = As<wmtcmd_render_setrasterizerstate>(cmd);
			[enc setTriangleFillMode:static_cast<MTLTriangleFillMode>(c.fill_mode)];
			[enc setCullMode:static_cast<MTLCullMode>(c.cull_mode)];
			[enc setDepthClipMode:static_cast<MTLDepthClipMode>(c.depth_clip_mode)];
			[enc setFrontFacingWinding:static_cast<MTLWinding>(c.winding)];
			/* `scole_scale` is the ABI's spelling of the slope scale. */
			[enc setDepthBias:c.depth_bias slopeScale:c.scole_scale clamp:c.depth_bias_clamp];
			break;
		}

		case WMTRenderCommandSetViewport:
		{
			const auto& c = As<wmtcmd_render_setviewport>(cmd);
			MTLViewport vp = {c.viewport.originX, c.viewport.originY, c.viewport.width,
			                  c.viewport.height, c.viewport.znear, c.viewport.zfar};
			RRR3D_TRACE_FIRST(4, "VIEWPORT x=%.1f y=%.1f w=%.1f h=%.1f z=%.2f..%.2f",
				vp.originX, vp.originY, vp.width, vp.height, vp.znear, vp.zfar);
			[enc setViewport:vp];
			break;
		}

		case WMTRenderCommandSetViewports:
		{
			const auto& c = As<wmtcmd_render_setviewports>(cmd);
			/* Layout-compatible with MTLViewport: six doubles in the same order. */
			[enc setViewports:static_cast<const MTLViewport*>(c.viewports.ptr)
			            count:c.viewport_count];
			break;
		}

		case WMTRenderCommandSetScissorRect:
		{
			const auto& c = As<wmtcmd_render_setscissorrect>(cmd);
			MTLScissorRect r = {c.scissor_rect.x, c.scissor_rect.y,
			                    c.scissor_rect.width, c.scissor_rect.height};
			RRR3D_TRACE_FIRST(4, "SCISSOR x=%llu y=%llu w=%llu h=%llu",
				(unsigned long long)r.x, (unsigned long long)r.y,
				(unsigned long long)r.width, (unsigned long long)r.height);
			[enc setScissorRect:r];
			break;
		}

		case WMTRenderCommandSetScissorRects:
		{
			const auto& c = As<wmtcmd_render_setscissorrects>(cmd);
			[enc setScissorRects:static_cast<const MTLScissorRect*>(c.scissor_rects.ptr)
			               count:c.rect_count];
			break;
		}

		case WMTRenderCommandSetVisibilityMode:
		{
			const auto& c = As<wmtcmd_render_setvisibilitymode>(cmd);
			[enc setVisibilityResultMode:static_cast<MTLVisibilityResultMode>(c.mode)
			                      offset:c.offset];
			break;
		}

		/* ---- draws ---- */
		case WMTRenderCommandDraw:
		{
			const auto& c = As<wmtcmd_render_draw>(cmd);
			[enc drawPrimitives:static_cast<MTLPrimitiveType>(c.primitive_type)
			        vertexStart:c.vertex_start
			        vertexCount:c.vertex_count
			      instanceCount:c.instance_count
			       baseInstance:c.base_instance];
			break;
		}

		case WMTRenderCommandDrawIndexed:
		{
			const auto& c = As<wmtcmd_render_draw_indexed>(cmd);
			[enc drawIndexedPrimitives:static_cast<MTLPrimitiveType>(c.primitive_type)
			                indexCount:c.index_count
			                 indexType:static_cast<MTLIndexType>(c.index_type)
			               indexBuffer:Unwrap<id<MTLBuffer>>(c.index_buffer)
			         indexBufferOffset:c.index_buffer_offset
			             instanceCount:c.instance_count
			                baseVertex:c.base_vertex
			              baseInstance:c.base_instance];
			break;
		}

		case WMTRenderCommandDrawIndirect:
		{
			const auto& c = As<wmtcmd_render_draw_indirect>(cmd);
			[enc drawPrimitives:static_cast<MTLPrimitiveType>(c.primitive_type)
			     indirectBuffer:Unwrap<id<MTLBuffer>>(c.indirect_args_buffer)
			indirectBufferOffset:c.indirect_args_offset];
			break;
		}

		case WMTRenderCommandDrawIndexedIndirect:
		{
			const auto& c = As<wmtcmd_render_draw_indexed_indirect>(cmd);
			[enc drawIndexedPrimitives:static_cast<MTLPrimitiveType>(c.primitive_type)
			                 indexType:static_cast<MTLIndexType>(c.index_type)
			               indexBuffer:Unwrap<id<MTLBuffer>>(c.index_buffer)
			         indexBufferOffset:c.index_buffer_offset
			            indirectBuffer:Unwrap<id<MTLBuffer>>(c.indirect_args_buffer)
			      indirectBufferOffset:c.indirect_args_offset];
			break;
		}

		case WMTRenderCommandDrawMeshThreadgroups:
		{
			const auto& c = As<wmtcmd_render_draw_meshthreadgroups>(cmd);
			[enc drawMeshThreadgroups:ToMTLSize(c.threadgroup_per_grid)
			threadsPerObjectThreadgroup:ToMTLSize(c.object_threadgroup_size)
			  threadsPerMeshThreadgroup:ToMTLSize(c.mesh_threadgroup_size)];
			break;
		}

		case WMTRenderCommandDrawMeshThreadgroupsIndirect:
		{
			const auto& c = As<wmtcmd_render_draw_meshthreadgroups_indirect>(cmd);
			[enc drawMeshThreadgroupsWithIndirectBuffer:Unwrap<id<MTLBuffer>>(c.indirect_args_buffer)
			                       indirectBufferOffset:c.indirect_args_offset
			                threadsPerObjectThreadgroup:ToMTLSize(c.object_threadgroup_size)
			                  threadsPerMeshThreadgroup:ToMTLSize(c.mesh_threadgroup_size)];
			break;
		}

		case WMTRenderCommandDispatchThreadsPerTile:
		{
			const auto& c = As<wmtcmd_render_dispatch_threads_per_tile>(cmd);
			[enc dispatchThreadsPerTile:MTLSizeMake(c.width, c.height, 1)];
			break;
		}

		/* ---- synchronisation ---- */
		case WMTRenderCommandWaitForFence:
		{
			const auto& c = As<wmtcmd_render_fence_op>(cmd);
			[enc waitForFence:Unwrap<id<MTLFence>>(c.fence)
			     beforeStages:static_cast<MTLRenderStages>(c.stages)];
			break;
		}

		case WMTRenderCommandUpdateFence:
		{
			const auto& c = As<wmtcmd_render_fence_op>(cmd);
			[enc updateFence:Unwrap<id<MTLFence>>(c.fence)
			     afterStages:static_cast<MTLRenderStages>(c.stages)];
			break;
		}

		case WMTRenderCommandMemoryBarrier:
		{
			const auto& c = As<wmtcmd_render_memory_barrier>(cmd);
			[enc memoryBarrierWithScope:static_cast<MTLBarrierScope>(c.scope)
			                afterStages:static_cast<MTLRenderStages>(c.stages_before)
			               beforeStages:static_cast<MTLRenderStages>(c.stages_after)];
			break;
		}

		/*
		 * DXMT emulates D3D11 geometry and tessellation shaders through these,
		 * driving Metal's mesh pipeline. Direct3D 9 has neither stage, so a D3D9
		 * workload cannot reach them -- and if one ever does, that is a bug worth
		 * hearing about rather than a frame quietly missing geometry.
		 */
		case WMTRenderCommandDXMTGeometryDraw:
		case WMTRenderCommandDXMTGeometryDrawIndexed:
		case WMTRenderCommandDXMTGeometryDrawIndirect:
		case WMTRenderCommandDXMTGeometryDrawIndexedIndirect:
		case WMTRenderCommandDXMTTessellationMeshDraw:
		case WMTRenderCommandDXMTTessellationMeshDrawIndexed:
		case WMTRenderCommandDXMTTessellationMeshDrawIndirect:
		case WMTRenderCommandDXMTTessellationMeshDrawIndexedIndirect:
			Unsupported("render (D3D11 geometry/tessellation emulation)", cmd->type);
			break;

		default:
			Unsupported("render", cmd->type);
			break;
		}
	}
}
