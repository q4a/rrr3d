/*
 * Presentation: the CAMetalLayer the backend renders into.
 *
 * On Wine this came from CreateMetalViewFromHWND, which found the NSView behind
 * a Win32 HWND. There is no Win32 here, so the "HWND" the engine carries is a
 * CAMetalLayer* -- created by SDL in src/RRR3d/RRR3d.cpp, passed through
 * IView::Desc::handle, and handed back to us untouched.
 *
 * A layer rather than a window, because SDL already made one. Asked for a Metal
 * window, SDL builds its own layer-hosting view and owns it; attaching a second
 * CAMetalLayer to the content view underneath means two layers competing, and
 * the one that draws is not the one on screen -- which is exactly the symptom
 * that produced a live process with an invisible window. Taking SDL's layer
 * and configuring it leaves ownership unambiguous.
 *
 * With no handle at all -- which is how this ran before the shell existed -- a
 * standalone layer is created instead. That is still a working swapchain:
 * nextDrawable hands back real textures and the backend renders into them, it
 * just cannot show them. Kept because it makes everything except the final
 * blit testable headlessly.
 */

#include "metalbridge_pipeline.h"

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

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

}

obj_handle_t CreateMetalViewFromHWND(intptr_t hwnd, obj_handle_t device, obj_handle_t* layer)
{
	if (layer)
		*layer = NULL_OBJECT_HANDLE;
	if (!device)
		return NULL_OBJECT_HANDLE;

	CAMetalLayer* metalLayer = nil;

	if (hwnd)
	{
		/* SDL's layer, already attached to a view that is already on screen. */
		metalLayer = (__bridge CAMetalLayer*)(void*)hwnd;
		[metalLayer retain];
	}
	else
	{
		metalLayer = [[CAMetalLayer alloc] init];
		/*
		 * Something has to be set before nextDrawable will produce anything;
		 * the backend overwrites this through MetalLayer_setProps once it knows
		 * the swapchain size. A real layer gets its size from its view.
		 */
		metalLayer.drawableSize = CGSizeMake(640, 480);
	}

	metalLayer.device = Unwrap<id<MTLDevice>>(device);
	metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	metalLayer.framebufferOnly = YES;

	if (layer)
		*layer = Wrap(metalLayer);

	/*
	 * The "view" handle. The layer stands in for it -- there is no NSView we
	 * own in either case -- so ReleaseMetalView has something to release and
	 * the ABI's two-handle shape holds.
	 */
	return Wrap(metalLayer);
}

void ReleaseMetalView(obj_handle_t view)
{
	if (view)
		[Unwrap<id>(view) release];
}

void MetalLayer_getProps(obj_handle_t layer, struct WMTLayerProps* props)
{
	if (!layer || !props)
		return;

	CAMetalLayer* metalLayer = Unwrap<CAMetalLayer*>(layer);
	props->device = Wrap(metalLayer.device);
	props->contents_scale = metalLayer.contentsScale;
	props->drawable_width = metalLayer.drawableSize.width;
	props->drawable_height = metalLayer.drawableSize.height;
	props->opaque = metalLayer.opaque;
	props->display_sync_enabled = metalLayer.displaySyncEnabled;
	props->framebuffer_only = metalLayer.framebufferOnly;
	props->pixel_format = static_cast<uint32_t>(metalLayer.pixelFormat);
}

void MetalLayer_setProps(obj_handle_t layer, const struct WMTLayerProps* props)
{
	if (!layer || !props)
		return;

	CAMetalLayer* metalLayer = Unwrap<CAMetalLayer*>(layer);
	if (props->device)
		metalLayer.device = Unwrap<id<MTLDevice>>(props->device);
	metalLayer.contentsScale = props->contents_scale > 0.0 ? props->contents_scale : 1.0;
	metalLayer.drawableSize = CGSizeMake(props->drawable_width, props->drawable_height);
	metalLayer.opaque = props->opaque;
	metalLayer.displaySyncEnabled = props->display_sync_enabled;
	metalLayer.framebufferOnly = props->framebuffer_only;
	metalLayer.pixelFormat = static_cast<MTLPixelFormat>(props->pixel_format);
}

obj_handle_t MetalLayer_nextDrawable(obj_handle_t layer)
{
	if (!layer)
		return NULL_OBJECT_HANDLE;

	/* Retained: nextDrawable is autoreleased, the ABI hands over ownership. */
	id<CAMetalDrawable> drawable = [[Unwrap<CAMetalLayer*>(layer) nextDrawable] retain];
	return Wrap(drawable);
}

obj_handle_t MetalDrawable_texture(obj_handle_t drawable)
{
	if (!drawable)
		return NULL_OBJECT_HANDLE;

	return Wrap([Unwrap<id<CAMetalDrawable>>(drawable) texture]);
}
