#pragma once

/*
 * Something drawn on top of the engine's frame, after the scene and before the
 * present.
 *
 * This exists because the engine had no seam for one. It never needed a seam:
 * the MFC map editor drew its panes with GDI into sibling HWNDs, so the only
 * thing inside the engine's window was the engine. An ImGui editor draws
 * through the same Direct3D device, and there was no point in the frame to do
 * it from -- IWorld exposes MainProgress, GetView, GetEdit and GetICamera, and
 * nothing that yields the device.
 *
 * The alternative was a second Metal pass on the same CAMetalLayer. The layer
 * and its drawables belong to d9mt, which recreates the swapchain on an extent
 * change and hands drawables to DXVK, so acquiring one from outside that stack
 * means the frame drawn is not the frame presented -- the failure
 * sdl_shell.cpp already warns about for a second CAMetalLayer.
 *
 * Lifetime is the caller's. World keeps a bare pointer and never owns it.
 */

struct IDirect3DDevice9;

namespace r3d
{

namespace game
{

class IOverlay
{
public:
	virtual ~IOverlay() {}

	/*
	 * Device lifetime, forwarded from the engine's own video-resource
	 * bookkeeping -- so an overlay's D3DPOOL_DEFAULT buffers and font texture
	 * are torn down and rebuilt on exactly the schedule every other default-pool
	 * resource in the engine is.
	 */
	virtual void OnLostDevice() = 0;
	virtual void OnResetDevice(IDirect3DDevice9* device) = 0;

	/*
	 * Called with the back buffer bound as render target 0, inside a scene.
	 * Draw and return; do not present.
	 */
	virtual void OnDraw(IDirect3DDevice9* device) = 0;
};

}

}
