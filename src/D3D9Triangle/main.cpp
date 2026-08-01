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
#include <cstdio>
#include <string>

struct Vertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
};

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
static int RenderToTexture(IDirect3DDevice9* dev, const void* tri, int stride)
{
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

	dev->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_XRGB(40, 40, 90), 1.0f, 0);

	if (SUCCEEDED(dev->BeginScene()))
	{
		dev->SetRenderState(D3DRS_LIGHTING, FALSE);
		dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
		dev->SetRenderState(D3DRS_ZENABLE, FALSE);
		dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
		dev->SetTexture(0, NULL);
		dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
		dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);

		HRESULT dr = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, stride);
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

	return 0;
}

int main(int argc, char** argv)
{
	const bool rttMode = argc > 1 && std::string(argv[1]) == "rtt";

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

	const Vertex tri[3] =
	{
		{ 400.0f,  80.0f, 0.5f, 1.0f, 0xffff0000 },
		{ 700.0f, 500.0f, 0.5f, 1.0f, 0xff00ff00 },
		{ 100.0f, 500.0f, 0.5f, 1.0f, 0xff0000ff },
	};

	if (rttMode)
	{
		const int rc = RenderToTexture(dev, tri, sizeof(Vertex));
		dev->Release();
		d3d->Release();
		SDL_Metal_DestroyView(view);
		SDL_DestroyWindow(window);
		SDL_Quit();
		return rc;
	}

	for (int frame = 0; frame < 400; ++frame)
	{
		SDL_Event e;
		while (SDL_PollEvent(&e))
			if (e.type == SDL_EVENT_QUIT) frame = 400;

		dev->Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER,
			D3DCOLOR_XRGB(40, 40, 90), 1.0f, 0);

		if (SUCCEEDED(dev->BeginScene()))
		{
			dev->SetRenderState(D3DRS_LIGHTING, FALSE);
			dev->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
			dev->SetRenderState(D3DRS_ZENABLE, FALSE);
			dev->SetFVF(D3DFVF_XYZRHW | D3DFVF_DIFFUSE);
			dev->SetTexture(0, NULL);
			dev->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
			dev->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
			HRESULT dr = dev->DrawPrimitiveUP(D3DPT_TRIANGLELIST, 1, tri, sizeof(Vertex));
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
					if (FILE* f = std::fopen("tri.tga", "wb"))
					{
						unsigned char h[18] = {0};
						h[2] = 2; h[12] = 800 & 0xff; h[13] = 800 >> 8;
						h[14] = 600 & 0xff; h[15] = 600 >> 8; h[16] = 32; h[17] = 0x20;
						std::fwrite(h, 1, 18, f);
						for (int y = 0; y < 600; ++y)
							std::fwrite(static_cast<char*>(r.pBits) + y * r.Pitch, 4, 800, f);
						std::fclose(f);
						std::printf("wrote tri.tga\n");
					}
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
	return 0;
}
