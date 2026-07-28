/*
 * A triangle through the winemetal ABI alone.
 *
 * src/D3D9Triangle proves the *stack* draws nothing. This goes one layer down
 * and drives src/MetalBridge directly -- no DXVK, no d9mt, no D3D9. It renders
 * into an offscreen texture and reads the pixels back, so it needs no window
 * and no eyes.
 *
 * Everything that could confound the D3D9 path is deliberately absent: no
 * argument buffers, no function constants, no vertex descriptor, no uniforms.
 * Vertices are read straight from a buffer by vertex_id and the fragment
 * shader returns a constant.
 *
 * If this draws, the bridge encodes draws correctly and the fault is above it.
 * If it does not, the fault is in the bridge and this is the reproducer.
 *
 * Needs tri.metallib beside the binary (built from tri.metal by CMake).
 */

#include "xplatform.h"

#include "metalbridge.h"
#include "metalbridge_pipeline.h"
#include "metalbridge_commands.h"
#include "d9mtmetal.h"

#include <cstdio>
#include <cstring>
#include <vector>

namespace
{

const uint32_t cWidth = 256;
const uint32_t cHeight = 256;

/* WMTPixelFormatBGRA8Unorm, which is Metal's own numbering. */
const uint32_t cFormatBGRA8Unorm = 80;

std::vector<char> ReadFile(const char* path)
{
	std::vector<char> data;
	if (std::FILE* f = std::fopen(path, "rb"))
	{
		std::fseek(f, 0, SEEK_END);
		const long size = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		data.resize(size > 0 ? size : 0);
		if (size > 0 && std::fread(data.data(), 1, size, f) != static_cast<size_t>(size))
			data.clear();
		std::fclose(f);
	}
	return data;
}

}

int main(int argc, char** argv)
{
	const char* mode = argc > 1 ? argv[1] : "vertexid";
	const bool useSpec    = std::strcmp(mode, "speccnst") == 0;
	const bool useNoCopy  = useSpec || std::strcmp(mode, "nocopy") == 0;
	const bool useArgBuf  = useNoCopy || std::strcmp(mode, "argbuf") == 0;
	const bool useStageIn = useArgBuf || std::strcmp(mode, "stagein") == 0;
	std::printf("mode: %s\n", mode);

	obj_handle_t devices = WMTCopyAllDevices();
	obj_handle_t device = (devices && NSArray_count(devices)) ? NSArray_object(devices, 0) : 0;
	if (!device)
	{
		std::printf("FAIL: no Metal device\n");
		return 1;
	}

	obj_handle_t queue = MTLDevice_newCommandQueue(device, 8);

	/* ---- render target ---- */

	WMTTextureInfo texInfo = {};
	texInfo.pixel_format = cFormatBGRA8Unorm;
	texInfo.width = cWidth;
	texInfo.height = cHeight;
	texInfo.depth = 1;
	texInfo.array_length = 1;
	texInfo.type = 2;                 /* MTLTextureType2D */
	texInfo.mipmap_level_count = 1;
	texInfo.sample_count = 1;
	texInfo.usage = 0x4 | 0x1;        /* RenderTarget | ShaderRead */
	texInfo.options = 0;              /* Shared */

	obj_handle_t target = MTLDevice_newTexture(device, &texInfo);
	if (!target)
	{
		std::printf("FAIL: newTexture\n");
		return 1;
	}

	/* ---- vertices: a big triangle in clip space ---- */

	const float verts[] =
	{
		 0.0f,  0.8f, 0.0f, 1.0f,
		 0.8f, -0.8f, 0.0f, 1.0f,
		-0.8f, -0.8f, 0.0f, 1.0f,
	};

	/*
	 * Two ways to get a buffer, because DXVK only ever uses the second:
	 *
	 *   default  memory.ptr null -- Metal allocates.
	 *   nocopy   memory.ptr set to VirtualAlloc'd pages, so MetalBridge takes
	 *            the newBufferWithBytesNoCopy path. Every DXVK buffer is made
	 *            this way, and nothing had tested it.
	 *
	 * Page-sized because newBufferWithBytesNoCopy requires both the pointer and
	 * the length to be page-aligned.
	 */
	WMTBufferInfo bufInfo = {};
	bufInfo.options = 0;

	if (useNoCopy)
	{
		const size_t page = 16384;
		void* pages = VirtualAlloc(NULL, page, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
		if (!pages)
		{
			std::printf("FAIL: VirtualAlloc\n");
			return 1;
		}
		std::memcpy(pages, verts, sizeof(verts));
		bufInfo.length = page;
		bufInfo.memory.ptr = pages;
		std::printf("nocopy: VirtualAlloc gave %p\n", pages);
	}
	else
	{
		bufInfo.length = sizeof(verts);
		bufInfo.memory.ptr = NULL;
	}

	obj_handle_t vbuf = MTLDevice_newBuffer(device, &bufInfo);
	if (!vbuf || !bufInfo.memory.ptr)
	{
		std::printf("FAIL: newBuffer\n");
		return 1;
	}
	if (!useNoCopy)
		std::memcpy(bufInfo.memory.ptr, verts, sizeof(verts));

	/* ---- pipeline ---- */

	std::vector<char> lib = ReadFile(useSpec ? "tri_speccnst.metallib" : useArgBuf ? "tri_argbuf.metallib" : (useStageIn ? "tri_stagein.metallib" : "tri.metallib"));
	if (lib.empty())
	{
		std::printf("FAIL: metallib not found beside the binary\n");
		return 1;
	}

	obj_handle_t library = MTLDevice_newLibrary(device, lib.data(), lib.size());
	if (!library)
	{
		std::printf("FAIL: newLibrary\n");
		return 1;
	}

	obj_handle_t vs = 0;
	if (useSpec)
	{
		/* Gate at 12, baked value at 1 -- the indices d9mt uses. */
		const uint32_t gate = 1;
		const uint32_t baked = 0x00E00000;

		WMTFunctionConstant consts[2] = {};
		consts[0].data.ptr = &gate;
		consts[0].type = WMTDataTypeUInt;
		consts[0].index = 12;
		consts[1].data.ptr = &baked;
		consts[1].type = WMTDataTypeUInt;
		consts[1].index = 1;

		obj_handle_t err = 0;
		vs = MTLLibrary_newFunctionWithConstants(library, "vs_main", consts, 2, &err);
		std::printf("speccnst: newFunctionWithConstants vs=%llx err=%llx\n",
			(unsigned long long)vs, (unsigned long long)err);
	}
	else
	{
		vs = MTLLibrary_newFunction(library, "vs_main");
	}
	obj_handle_t fs = MTLLibrary_newFunction(library, "fs_main");
	if (!vs || !fs)
	{
		std::printf("FAIL: newFunction vs=%llx fs=%llx\n",
			(unsigned long long)vs, (unsigned long long)fs);
		return 1;
	}

	WMTRenderPipelineInfo psoInfo = {};
	psoInfo.vertex_function = vs;
	psoInfo.fragment_function = fs;
	psoInfo.rasterization_enabled = true;
	psoInfo.raster_sample_count = 1;
	psoInfo.input_primitive_topology = static_cast<WMTPrimitiveTopologyClass>(0);   /* Unspecified */
	psoInfo.colors[0].pixel_format = static_cast<WMTPixelFormat>(cFormatBGRA8Unorm);
	psoInfo.colors[0].write_mask = 0xf;
	psoInfo.colors[0].blending_enabled = false;

	obj_handle_t pso = 0;

	/*
	 * Two ways to build the pipeline, selected by argv[1]:
	 *
	 *   (default)  MetalBridge's own newRenderPipelineState, no vertex
	 *              descriptor -- the shader indexes the buffer by vertex_id.
	 *   stagein    d9mtmetal's newRenderPSO with a vertex descriptor, the
	 *              path every real D3D9 draw takes.
	 *
	 * Adding one layer at a time is the point: if the first works and the
	 * second does not, the vertex-descriptor path is the fault.
	 */
	if (useStageIn)
	{
		d9mt_pso_info info = {};
		info.vertex_function = vs;
		info.fragment_function = fs;
		info.colors[0].pixel_format = cFormatBGRA8Unorm;
		info.colors[0].write_mask = 0xf;
		info.raster_sample_count = 1;

		info.num_attributes = 1;
		info.attributes[0].format = 31;        /* MTLVertexFormatFloat4 */
		info.attributes[0].offset = 0;
		info.attributes[0].buffer_index = 14;  /* what d9mt binds vertices at */
		info.attributes[0].location = 0;

		info.num_layouts = 1;
		info.layouts[0].buffer_index = 14;
		info.layouts[0].stride = 16;
		info.layouts[0].step_function = 1;     /* per vertex */
		info.layouts[0].step_rate = 1;

		d9mt_newpso_params params = {};
		params.device = device;
		params.info_ptr = uint64_t(uintptr_t(&info));

		const int status = D9MT_UnixCall(D9MT_FUNC_NEW_RENDER_PSO, &params);
		std::printf("d9mtmetal newRenderPSO: status=%d pso=%llx err=%llx\n",
			status, (unsigned long long)params.ret_pso, (unsigned long long)params.ret_error);
		pso = params.ret_pso;
	}
	else
	{
		obj_handle_t psoError = 0;
		pso = MTLDevice_newRenderPipelineState(device, &psoInfo, &psoError);
	}

	if (!pso)
	{
		std::printf("FAIL: pipeline creation\n");
		return 1;
	}

	/* ---- encode ---- */

	obj_handle_t pool = NSAutoreleasePool_alloc_init();
	obj_handle_t cmdbuf = MTLCommandQueue_commandBuffer(queue);

	WMTRenderPassInfo pass = {};
	pass.colors[0].texture = target;
	pass.colors[0].load_action = static_cast<WMTLoadAction>(2);      /* Clear */
	pass.colors[0].store_action = static_cast<WMTStoreAction>(1);     /* Store */
	pass.colors[0].clear_color = {0.0, 0.0, 1.0, 1.0};   /* blue */
	pass.render_target_width = cWidth;
	pass.render_target_height = cHeight;
	pass.render_target_array_length = 1;
	pass.default_raster_sample_count = 1;

	obj_handle_t enc = MTLCommandBuffer_renderCommandEncoder(cmdbuf, &pass);
	if (!enc)
	{
		std::printf("FAIL: renderCommandEncoder\n");
		return 1;
	}

	/*
	 * Argument buffer layer: a buffer holding a pointer to another buffer, the
	 * way d9mt hands constants to generated shaders. Metal cannot see anything
	 * reached this way unless useResource marks it resident, so that command is
	 * part of what is being tested.
	 */
	obj_handle_t constsBuf = 0;
	obj_handle_t argBuf = 0;
	if (useArgBuf)
	{
		const float scale[4] = {1.0f, 1.0f, 1.0f, 1.0f};

		WMTBufferInfo ci = {};
		ci.length = sizeof(scale);
		ci.memory.ptr = NULL;
		constsBuf = MTLDevice_newBuffer(device, &ci);
		std::memcpy(ci.memory.ptr, scale, sizeof(scale));

		WMTBufferInfo ai = {};
		ai.length = 2 * sizeof(uint64_t);
		ai.memory.ptr = NULL;
		argBuf = MTLDevice_newBuffer(device, &ai);

		uint64_t* ab = static_cast<uint64_t*>(ai.memory.ptr);
		ab[0] = 0;                 /* id(0): null, as slot 31 is in the real path */
		ab[1] = ci.gpu_address;    /* id(1): the constants the shader reads */

		std::printf("argbuf: consts gpuAddr=0x%llx\n", (unsigned long long)ci.gpu_address);
	}

	/* The command stream, exactly as d9mt builds it. */
	wmtcmd_render_setpso setPso = {};
	setPso.type = WMTRenderCommandSetPSO;
	setPso.pso = pso;

	wmtcmd_render_setbuffer setVb = {};
	setVb.type = WMTRenderCommandSetVertexBuffer;
	setVb.buffer = vbuf;
	setVb.offset = 0;
	setVb.index = useStageIn ? 14 : 0;

	wmtcmd_render_draw draw = {};
	draw.type = WMTRenderCommandDraw;
	draw.primitive_type = static_cast<WMTPrimitiveType>(3);             /* MTLPrimitiveTypeTriangle */
	draw.vertex_start = 0;
	draw.vertex_count = 3;
	draw.instance_count = 1;
	draw.base_instance = 0;

	wmtcmd_render_useresource useRes = {};
	useRes.type = WMTRenderCommandUseResource;
	useRes.resource = constsBuf;
	useRes.usage = static_cast<WMTResourceUsage>(3);   /* Read | Write */
	useRes.stages = static_cast<WMTRenderStages>(3);   /* Vertex | Fragment */

	wmtcmd_render_setbuffer setAb = {};
	setAb.type = WMTRenderCommandSetVertexBuffer;
	setAb.buffer = argBuf;
	setAb.offset = 0;
	setAb.index = 0;

	if (useArgBuf)
	{
		setPso.next.ptr = &useRes;
		useRes.next.ptr = &setAb;
		setAb.next.ptr = &setVb;
	}
	else
	{
		setPso.next.ptr = &setVb;
	}
	setVb.next.ptr = &draw;
	draw.next.ptr = NULL;

	MTLRenderCommandEncoder_encodeCommands(enc,
		reinterpret_cast<const wmtcmd_base*>(&setPso));

	MTLCommandEncoder_endEncoding(enc);
	MTLCommandBuffer_commit(cmdbuf);
	MTLCommandBuffer_waitUntilCompleted(cmdbuf);
	NSObject_release(pool);

	/* ---- read back: blit the target into a buffer, since the ABI has no getBytes ---- */

	WMTBufferInfo readInfo = {};
	readInfo.length = cWidth * cHeight * 4;
	readInfo.options = 0;
	readInfo.memory.ptr = NULL;
	obj_handle_t readback = MTLDevice_newBuffer(device, &readInfo);

	obj_handle_t pool2 = NSAutoreleasePool_alloc_init();
	obj_handle_t copyBuf = MTLCommandQueue_commandBuffer(queue);
	obj_handle_t blit = MTLCommandBuffer_blitCommandEncoder(copyBuf);

	wmtcmd_blit_copy_from_texture_to_buffer copy = {};
	copy.type = WMTBlitCommandCopyFromTextureToBuffer;
	copy.src = target;
	copy.slice = 0;
	copy.level = 0;
	copy.origin = {0, 0, 0};
	copy.size = {cWidth, cHeight, 1};
	copy.dst = readback;
	copy.offset = 0;
	copy.bytes_per_row = cWidth * 4;
	copy.bytes_per_image = cWidth * cHeight * 4;
	copy.next.ptr = NULL;

	MTLBlitCommandEncoder_encodeCommands(blit, reinterpret_cast<const wmtcmd_base*>(&copy));
	MTLCommandEncoder_endEncoding(blit);
	MTLCommandBuffer_commit(copyBuf);
	MTLCommandBuffer_waitUntilCompleted(copyBuf);
	NSObject_release(pool2);

	const unsigned char* pixels = static_cast<const unsigned char*>(readInfo.memory.ptr);
	if (!pixels)
	{
		std::printf("FAIL: readback buffer not mapped\n");
		return 1;
	}

	auto at = [&](uint32_t x, uint32_t y)
	{
		const unsigned char* p = &pixels[(y * cWidth + x) * 4];
		std::printf("(%u,%u) = B%3u G%3u R%3u A%3u\n", x, y, p[0], p[1], p[2], p[3]);
	};

	std::printf("centre should be GREEN, corner should be BLUE:\n");
	at(cWidth / 2, cHeight / 2);
	at(4, 4);

	const unsigned char* centre = &pixels[((cHeight / 2) * cWidth + cWidth / 2) * 4];
	const bool green = centre[1] > 200 && centre[2] < 60;
	std::printf("\n%s\n", green
		? "PASS: the bridge encodes draws correctly."
		: "FAIL: the draw produced nothing -- the fault is in src/MetalBridge.");

	return green ? 0 : 1;
}
