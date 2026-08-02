/*
 * Does Dear ImGui's D3D9 backend draw on this stack?
 *
 * Phase 12 replaces the MFC map editor with one built on Dear ImGui, drawn
 * through the same Direct3D device the engine already owns -- so the panes land
 * inside the engine's own frame instead of contending with d9mt for the Metal
 * layer. That choice rests on one unproven assumption, and this program exists
 * to test it before anything is built on top.
 *
 * THE ASSUMPTION. imgui_impl_dx9 draws with FIXED-FUNCTION TnL: D3DFVF_XYZRHW,
 * no vertex or pixel shader, and D3DTSS_COLOROP to modulate the font texture
 * against the vertex colour. DXVK implements all of that -- d3d9_fixed_function.cpp
 * generates SPIR-V which spirv-cross turns into MSL, exactly as it does for a
 * translated shader -- but the engine binds a shader for every draw it makes.
 * There are zero SetVertexShader/SetPixelShader calls in Rock3dEngine; every
 * draw goes through ID3DXEffect::BeginPass, which sets both. So DXVK's
 * fixed-function generator has almost certainly never executed in this build,
 * and "never executed" is not the same as "works".
 *
 * If it fails it will fail the way phase 7 did: commitGraphicsState returns
 * false, the draw is skipped, DrawIndexedPrimitive returns S_OK, and the UI is
 * simply absent. Hence pixels, not HRESULTs.
 *
 * THREE CHECKS, because the interesting failure is partial:
 *
 *   1. A pixel inside a known-colour button equals that colour. This is the
 *      untextured path -- flat geometry with vertex colour.
 *   2. A pixel on a glyph differs from the window background. This is the
 *      TEXTURED path, and it is separate because a missing D3DTSS_COLOROP
 *      gives untextured quads or invisible text while check 1 still passes.
 *      That is the failure most likely to be mistaken for success.
 *   3. A pixel well outside the ImGui window is still the clear colour, so
 *      nothing scribbled over the rest of the target.
 *
 * Renders to a program-owned render target and reads it back, and writes
 * imgui.tga to look at -- a CAMetalLayer's contents never appear in a
 * screenshot.
 */

#include "xplatform.h"
#include "d3d9.h"

#include "imgui.h"
#include "imgui_impl_dx9.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace
{

const int cWidth = 800;
const int cHeight = 600;

const D3DCOLOR cClear = D3DCOLOR_XRGB(40, 40, 90);

/* Where the ImGui window is put, and how big -- so the checks below can name
   pixels that must be inside it. */
const float cWindowX = 100.0f;
const float cWindowY = 100.0f;
const float cWindowW = 400.0f;
const float cWindowH = 300.0f;

/* The button's colour. Chosen to be unlike both the clear colour and ImGui's
   default greys, so a match cannot be a coincidence. */
const ImVec4 cButtonColor(1.0f, 0.35f, 0.0f, 1.0f);

D3DCOLOR At(const D3DLOCKED_RECT& r, int x, int y)
{
	return *reinterpret_cast<const D3DCOLOR*>(
		static_cast<const char*>(r.pBits) + y * r.Pitch + x * 4);
}

bool Near(D3DCOLOR got, D3DCOLOR want, int tolerance)
{
	for (int shift = 0; shift <= 16; shift += 8)
		if (std::abs(int((got >> shift) & 0xff) - int((want >> shift) & 0xff)) > tolerance)
			return false;
	return true;
}

void WriteTga(const char* path, const D3DLOCKED_RECT& r, int w, int h)
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

}

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	/*
	 * Synchronous pipeline compilation, for the reason D3D9Triangle sets it:
	 * d9mt skips a draw whose pipeline state is not yet compiled, silently, and
	 * a program that draws once and reads the result sees a blank target. Set
	 * before anything touches D3D9, and not overwritten, so
	 * `D9MT_ASYNC=1 ./D3D9ImGui` still reproduces that behaviour.
	 */
	setenv("D9MT_ASYNC", "0", 0);

	if (!SDL_Init(SDL_INIT_VIDEO))
	{
		std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("imgui over d3d9", cWidth, cHeight, SDL_WINDOW_METAL);
	SDL_MetalView view = SDL_Metal_CreateView(window);
	void* layer = SDL_Metal_GetLayer(view);

	IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
	if (!d3d)
	{
		std::fprintf(stderr, "Direct3DCreate9 failed\n");
		return 1;
	}

	D3DPRESENT_PARAMETERS pp = {};
	pp.BackBufferWidth = cWidth;
	pp.BackBufferHeight = cHeight;
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
		std::fprintf(stderr, "CreateDevice failed 0x%08x\n", (unsigned)hr);
		return 1;
	}
	std::printf("device created\n");

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(float(cWidth), float(cHeight));
	io.DeltaTime = 1.0f / 60.0f;
	/* No .ini: this is a test, and a file written beside the binary would make
	   the second run differ from the first. */
	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	if (!ImGui_ImplDX9_Init(dev))
	{
		std::fprintf(stderr, "FAIL ImGui_ImplDX9_Init\n");
		return 1;
	}

	/*
	 * A render target this program owns, rather than the swapchain.
	 *
	 * The same reasoning as D3D9Triangle's rtt mode: it separates "the draw did
	 * not happen" from "the present did not happen", and it can be read back
	 * without a Present at all.
	 */
	IDirect3DSurface9* rt = NULL;
	if (FAILED(dev->CreateRenderTarget(cWidth, cHeight, D3DFMT_A8R8G8B8,
			D3DMULTISAMPLE_NONE, 0, FALSE, &rt, NULL)) || !rt)
	{
		std::fprintf(stderr, "CreateRenderTarget failed\n");
		return 1;
	}

	IDirect3DSurface9* oldRt = NULL;
	dev->GetRenderTarget(0, &oldRt);
	dev->SetRenderTarget(0, rt);
	dev->SetDepthStencilSurface(NULL);   /* no depth on this target */

	/*
	 * Several frames, not one. The font atlas is uploaded on the first
	 * NewFrame, and ImGui's own auto-sizing settles over a frame or two -- so a
	 * single-frame capture can miss content that is present a frame later, and
	 * would be a flaky test rather than a strict one.
	 */
	for (int frame = 0; frame < 4; ++frame)
	{
		ImGui_ImplDX9_NewFrame();
		ImGui::NewFrame();

		ImGui::SetNextWindowPos(ImVec2(cWindowX, cWindowY), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(cWindowW, cWindowH), ImGuiCond_Always);
		ImGui::Begin("smoke", NULL,
			ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

		/* Textured: glyphs come from the font atlas. */
		ImGui::TextUnformatted("MMMMMMMMMMMMMMMM");

		/* Untextured: a flat filled rectangle in a known colour. */
		ImGui::ColorButton("##swatch", cButtonColor,
			ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoBorder,
			ImVec2(200.0f, 100.0f));

		ImGui::End();
		ImGui::Render();

		dev->Clear(0, NULL, D3DCLEAR_TARGET, cClear, 1.0f, 0);
		if (SUCCEEDED(dev->BeginScene()))
		{
			ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
			dev->EndScene();
		}
	}

	int rc = 1;

	IDirect3DSurface9* copy = NULL;
	if (SUCCEEDED(dev->CreateOffscreenPlainSurface(cWidth, cHeight, D3DFMT_A8R8G8B8,
			D3DPOOL_SYSTEMMEM, &copy, NULL)) &&
		SUCCEEDED(dev->GetRenderTargetData(rt, copy)))
	{
		D3DLOCKED_RECT r;
		if (SUCCEEDED(copy->LockRect(&r, NULL, D3DLOCK_READONLY)))
		{
			WriteTga("imgui.tga", r, cWidth, cHeight);

			rc = 0;

			/* 3: outside the window, the clear must have survived. */
			const D3DCOLOR outside = At(r, 20, 20);
			if (!Near(outside, cClear, 2))
			{
				std::printf("FAIL outside (20,20) is 0x%08x, want clear 0x%08x\n",
					outside, cClear);
				rc = 1;
			}

			/*
			 * 1: the swatch. Its top-left is a little below the text line, and
			 * the sample is taken well inside it so padding and rounding cannot
			 * reach. Tolerance is generous because ImGui's style may apply a
			 * slight tint, and the point is "this colour, not the background".
			 */
			const D3DCOLOR wantButton = D3DCOLOR_XRGB(
				int(cButtonColor.x * 255.0f + 0.5f),
				int(cButtonColor.y * 255.0f + 0.5f),
				int(cButtonColor.z * 255.0f + 0.5f));

			bool foundButton = false;
			D3DCOLOR sawButton = 0;
			for (int y = int(cWindowY) + 40; y < int(cWindowY + cWindowH) - 10 && !foundButton; ++y)
			{
				for (int x = int(cWindowX) + 20; x < int(cWindowX) + 180; ++x)
				{
					const D3DCOLOR c = At(r, x, y);
					if (Near(c, wantButton, 24))
					{
						foundButton = true;
						sawButton = c;
						break;
					}
				}
			}

			if (foundButton)
				std::printf("PASS untextured: found the swatch, 0x%08x\n", sawButton);
			else
			{
				std::printf("FAIL untextured: no pixel matching 0x%08x inside the window\n",
					wantButton);
				rc = 1;
			}

			/*
			 * 2: the text. The window background is a flat colour, so any
			 * variation along the text line means glyphs were rasterised --
			 * which is only true if the font texture was sampled.
			 */
			const int textY = int(cWindowY) + 8;
			D3DCOLOR first = At(r, int(cWindowX) + 10, textY);
			bool varied = false;
			for (int x = int(cWindowX) + 10; x < int(cWindowX) + 300; ++x)
			{
				for (int y = textY; y < textY + 14; ++y)
				{
					if (!Near(At(r, x, y), first, 8))
					{
						varied = true;
						break;
					}
				}
				if (varied)
					break;
			}

			if (varied)
				std::printf("PASS textured: glyphs are present on the text line\n");
			else
			{
				std::printf("FAIL textured: the text line is a flat 0x%08x -- "
					"the font texture was not sampled\n", first);
				rc = 1;
			}

			copy->UnlockRect();
		}
	}
	else
	{
		std::fprintf(stderr, "FAIL readback\n");
	}

	if (copy) copy->Release();
	if (oldRt) { dev->SetRenderTarget(0, oldRt); oldRt->Release(); }
	rt->Release();

	ImGui_ImplDX9_Shutdown();
	ImGui::DestroyContext();

	dev->Release();
	d3d->Release();
	SDL_Metal_DestroyView(view);
	SDL_DestroyWindow(window);
	SDL_Quit();

	std::printf(rc == 0 ? "D3D9ImGui: ok\n" : "D3D9ImGui: FAILED\n");
	return rc;
}
