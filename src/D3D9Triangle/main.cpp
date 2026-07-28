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

struct Vertex
{
	float x, y, z, rhw;
	D3DCOLOR color;
};

int main()
{
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

		dev->Present(NULL, NULL, NULL, NULL);

		/* Read back frame 100 so the result is inspectable without a screenshot. */
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
	}

	dev->Release();
	d3d->Release();
	SDL_Metal_DestroyView(view);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}
