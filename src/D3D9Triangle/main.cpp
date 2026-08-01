/*
 * Minimal D3D9 triangle against src/D3D9Metal, with an SDL window.
 *
 * The native equivalent of d9mt's test/triangle.c, which is Win32-only. Nothing
 * of the game is involved: if this renders, the backend works and the fault is
 * in how the engine drives it. If it does not, the fault is in the backend or
 * the winemetal bridge, and this is a 90-line reproducer instead of a 130k-line
 * one.
 */

#include "xplatform.h"
#include "d3d9.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

struct Vertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
};

static const D3DCOLOR kClear = D3DCOLOR_XRGB(40, 40, 90);

static const Vertex kTri[3] =
{
	{ 400.0f,  80.0f, 0.5f, 1.0f, 0xffff0000 },
	{ 700.0f, 500.0f, 0.5f, 1.0f, 0xff00ff00 },
	{ 100.0f, 500.0f, 0.5f, 1.0f, 0xff0000ff },
};

/*
 * The Gouraud-interpolated colour the rasteriser owes at (x, y), from the same
 * vertex data the draw uses -- so the expected value is derived rather than a
 * hard-coded triple that would have to be re-derived if the triangle moved.
 */
static D3DCOLOR ExpectedAt(float x, float y)
{
	const Vertex& a = kTri[0];
	const Vertex& b = kTri[1];
	const Vertex& c = kTri[2];

	const float det = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
	const float w0 = ((b.y - c.y) * (x - c.x) + (c.x - b.x) * (y - c.y)) / det;
	const float w1 = ((c.y - a.y) * (x - c.x) + (a.x - c.x) * (y - c.y)) / det;
	const float w2 = 1.0f - w0 - w1;

	D3DCOLOR out = 0xff000000;
	for (int shift = 0; shift <= 16; shift += 8)
	{
		const float v = w0 * ((a.color >> shift) & 0xff)
		              + w1 * ((b.color >> shift) & 0xff)
		              + w2 * ((c.color >> shift) & 0xff);
		out |= D3DCOLOR(v + 0.5f) << shift;
	}
	return out;
}

static bool Near(D3DCOLOR got, D3DCOLOR want)
{
	for (int shift = 0; shift <= 16; shift += 8)
		if (std::abs(int((got >> shift) & 0xff) - int((want >> shift) & 0xff)) > 2)
			return false;
	return true;
}

/*
 * Reads the surface rather than trusting HRESULTs.
 *
 * Every signal short of the pixels says success even when nothing is drawn:
 * the device is created, DrawPrimitiveUP returns S_OK, the readback succeeds
 * and a 1.9 MB file is written. So this is the only check that means anything,
 * and it is the program's exit status.
 */
static int CheckTriangle(const char* what, const D3DLOCKED_RECT& r, int w, int h)
{
	auto at = [&] (int x, int y) {
		return *reinterpret_cast<const D3DCOLOR*>(
			static_cast<const char*>(r.pBits) + y * r.Pitch + x * 4);
	};

	/* Outside the triangle: the clear must have survived the draw. */
	const D3DCOLOR corner = at(4, 4);
	if (!Near(corner, kClear))
	{
		std::printf("%s: FAIL corner (4,4) is 0x%08x, want clear 0x%08x\n",
			what, corner, kClear);
		return 1;
	}

	/* Inside it: the interpolated diffuse, which the clear colour is not. */
	const int cx = w / 2, cy = h / 2;
	const D3DCOLOR got = at(cx, cy);
	const D3DCOLOR want = ExpectedAt(float(cx) + 0.5f, float(cy) + 0.5f);

	if (!Near(got, want))
	{
		std::printf("%s: FAIL centre (%d,%d) is 0x%08x, want 0x%08x%s\n",
			what, cx, cy, got, want,
			Near(got, kClear) ? " -- the clear colour, so the draw did not land" : "");
		return 1;
	}

	std::printf("%s: PASS centre 0x%08x matches the interpolated diffuse\n", what, got);
	return 0;
}

/*
 * Writes a 32-bit TGA from a locked surface, so a result is inspectable
 * without a screenshot -- a CAMetalLayer's contents never appear in one.
 */
static void WriteTga(const char* path, const D3DLOCKED_RECT& r, int w, int h)
{
	FILE* f = std::fopen(path, "wb");
	if (!f)
		return;

	unsigned char hdr[18] = {0};
	hdr[2] = 2;
	hdr[12] = w & 0xff; hdr[13] = w >> 8;
	hdr[14] = h & 0xff; hdr[15] = h >> 8;
	hdr[16] = 32; hdr[17] = 0x20;

	std::fwrite(hdr, 1, 18, f);
	for (int y = 0; y < h; ++y)
		std::fwrite(static_cast<char*>(r.pBits) + y * r.Pitch, 4, w, f);

	std::fclose(f);
	std::printf("wrote %s\n", path);
}

/*
 * The same triangle, drawn to a render target this program owns rather than to
 * the swapchain's back buffer.
 *
 * This is the experiment that separates two candidates for a missing triangle:
 * if it appears here, rasterisation works and the fault is in the swapchain or
 * present path; if it is missing here too, the draw is not reaching any render
 * target and the swapchain is innocent.
 */
static int RenderToTexture(IDirect3DDevice9* dev)
{
	int rc = 1;

	IDirect3DSurface9* rt = NULL;
	if (FAILED(dev->CreateRenderTarget(800, 600, D3DFMT_A8R8G8B8,
			D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL)) || !rt)
	{
		std::fprintf(stderr, "CreateRenderTarget failed\n");
		return 1;
	}

	IDirect3DSurface9* oldRt = NULL;
	dev->GetRenderTarget(0, &oldRt);
	dev->SetRenderTarget(0, rt);

	/* No depth buffer on this target, so depth testing must be off. */
	dev->SetDepthStencilSurface(NULL);

	dev->Clear(0, NULL, D3DCLEAR_TARGET, kClear, 1.0f, 0);

	if (SUCCEEDED(dev->BeginScene()))
	{
		dev->SetRenderState(D3DRS_LIGHTING, FALSE);
		dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		dev->SetRenderState(D3DRS_ZENABLE, FALSE);
		dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
		dev->SetTexture(0, NULL);
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

		HRESULT dr = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTri, sizeof(Vertex));
		std::printf("rtt DrawPrimitiveUP hr=0x%08x\n", (unsigned)dr);

		dev->EndScene();
	}

	IDirect3DSurface9* copy = NULL;
	if (SUCCEEDED(dev->CreateOffscreenPlainSurface(800, 600, D3DFMT_A8R8G8B8,
			D3DPOOL_SYSTEMMEM, &copy, NULL)) &&
		SUCCEEDED(dev->GetRenderTargetData(rt, copy)))
	{
		D3DLOCKED_RECT r;
		if (SUCCEEDED(copy->LockRect(&r, NULL, D3DLOCK_READONLY)))
		{
			WriteTga("tri_rtt.tga", r, 800, 600);
			rc = CheckTriangle("rtt", r, 800, 600);
			copy->UnlockRect();
		}
	}
	else
	{
		std::fprintf(stderr, "rtt readback failed\n");
	}

	if (copy) copy->Release();
	if (oldRt) { dev->SetRenderTarget(0, oldRt); oldRt->Release(); }
	rt->Release();

	return rc;
}

int main(int argc, char** argv)
{
	const bool rttMode = argc > 1 && std::string(argv[1]) == "rtt";

	/*
	 * Compile pipelines synchronously.
	 *
	 * d9mt mirrors dxvk-async: a pipeline state seen for the first time is
	 * handed to a background worker and THE DRAW IS SKIPPED until it is hot
	 * (d9mt_context.cpp, getRenderPso -- "pso stays 0 until the worker
	 * finishes; the draw site skips until then"). Nothing reports this. The
	 * draw returns S_OK, the clear still lands as the render pass's load
	 * action, and the surface reads back as flat clear colour.
	 *
	 * That is fine for a game, which drops a frame or two of new geometry and
	 * moves on. It is fatal for a test that draws once and reads the result,
	 * and it is why this program appeared to render nothing at all.
	 *
	 * Set before anything touches D3D9: d9mt caches the answer in a
	 * function-local static on first use. Not overwritten, so
	 * `D9MT_ASYNC=1 ./D3D9Triangle` still reproduces the async behaviour.
	 */
	setenv("D9MT_ASYNC", "0", 0);

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("d3d9 triangle", 800, 600, SDL_WINDOW_METAL);
	SDL_MetalView view = SDL_Metal_CreateView(window);
	void* layer = SDL_Metal_GetLayer(view);
	std::printf("layer = %p\n", layer);

	IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d)
	{
		std::fprintf(stderr, "Direct3DCreate9 failed\n");
		return 1;
	}

	D3DPRESENT_PARAMETERS pp = {};
	pp.BackBufferWidth = 800;
	pp.BackBufferHeight = 600;
	pp.BackBufferFormat = D3DFMT_A8R8G8B8;
	pp.BackBufferCount = 1;
	pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	pp.hDeviceWindow = reinterpret_cast<HWND>(layer);
	pp.Windowed = TRUE;
	pp.EnableAutoDepthStencil = TRUE;
	pp.AutoDepthStencilFormat = D3DFMT_D24X8;
	pp.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	IDirect3DDevice9* dev = NULL;
	HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
		reinterpret_cast<HWND>(layer), D3DCREATE_HARDWARE_VERTEXPROCESSING, &pp, &dev);
	if (FAILED(hr) || !dev)
	{
		std::fprintf(stderr, "CreateDevice failed 0x%08x\n", hr);
		return 1;
	}
	std::printf("device created\n");

	if (rttMode)
	{
		const int rc = RenderToTexture(dev);
		dev->Release();
		d3d->Release();
		SDL_Metal_DestroyView(view);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return rc;
	}

	int rc = 1;

	for (int frame = 0; frame < 400; ++frame)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
			if (e.type == SDL_EVENT_QUIT) frame = 400;

		dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, kClear, 1.0f, 0);

		if (SUCCEEDED(dev->BeginScene()))
		{
			dev->SetRenderState(D3DRS_LIGHTING, FALSE);
			dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			dev->SetRenderState(D3DRS_ZENABLE, FALSE);
			dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
			dev->SetTexture(0, NULL);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			HRESULT dr = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, kTri, sizeof(Vertex));
			if (frame == 0)
				std::printf("DrawPrimitiveUP hr=0x%08x\n", dr);
			dev->EndScene();
		}

		/*
		 * Read back BEFORE Present, not after.
		 *
		 * The swap effect is D3DSWAPEFFECT_DISCARD, which means exactly what it
		 * says: once Present has been called the back buffer's contents are
		 * undefined. Reading afterwards returns a surface that has been cleared
		 * and not drawn to, so the triangle is missing and everything else --
		 * the device, the draw call's HRESULT, the file being written -- still
		 * looks like success.
		 */
		if (frame == 100)
		{
			IDirect3DSurface9* back = NULL;
			IDirect3DSurface9* copy = NULL;
			dev->GetRenderTarget(0, &back);
			if (back)
			{
				D3DSURFACE_DESC d = {};
				back->GetDesc(&d);
				std::printf("backbuffer is %ux%u fmt=%d\n", d.Width, d.Height, (int)d.Format);
				D3DVIEWPORT9 vp = {};
				dev->GetViewport(&vp);
				std::printf("viewport %u,%u %ux%u\n", vp.X, vp.Y, vp.Width, vp.Height);
			}
			if (back && SUCCEEDED(dev->CreateOffscreenPlainSurface(800, 600,
					D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &copy, NULL)) &&
				SUCCEEDED(dev->GetRenderTargetData(back, copy)))
			{
				D3DLOCKED_RECT r;
				if (SUCCEEDED(copy->LockRect(&r, NULL, D3DLOCK_READONLY)))
				{
					WriteTga("tri.tga", r, 800, 600);
					rc = CheckTriangle("swapchain", r, 800, 600);
					copy->UnlockRect();
				}
			}
			if (copy) copy->Release();
			if (back) back->Release();
		}

		dev->Present(NULL, NULL, NULL, NULL);
	}

	dev->Release();
	d3d->Release();
	SDL_Metal_DestroyView(view);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return rc;
}
