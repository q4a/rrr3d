/*
 * Presentation: the CAMetalLayer the backend renders into.
 *
 * On Wine this came from CreateMetalViewFromHWND, which found the NSView behind
 * a Win32 HWND and attached a layer to it. There is no HWND here and, until the
 * SDL shell lands, no window either -- so the layer is created standalone.
 *
 * A detached CAMetalLayer is still a working swapchain: nextDrawable hands back
 * real textures of the configured size and the backend renders into them
 * normally. What it cannot do is put them on a screen. That is exactly the
 * split we want for now -- it makes every part of the pipeline except the final
 * blit to a window testable, and a real NSView drops in later by setting
 * `layer` on it.
 *
 * When an HWND is eventually passed, it will be an SDL window handle and this
 * becomes a lookup rather than a construction.
 */

#include "metalbridge_pipeline.h"

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

	CAMetalLayer* metalLayer = [[CAMetalLayer alloc] init];
	metalLayer.device = Unwrap<id<MTLDevice>>(device);
	metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
	metalLayer.framebufferOnly = YES;
	/*
	 * Something has to be set before nextDrawable will produce anything; the
	 * backend overwrites this through MetalLayer_setProps once it knows the
	 * swapchain size.
	 */
	metalLayer.drawableSize = CGSizeMake(640, 480);

	if (layer)
		*layer = Wrap(metalLayer);

	/*
	 * The "view" handle. With no NSView to own, the layer stands in for it, so
	 * ReleaseMetalView has something meaningful to release and the ABI's
	 * two-handle shape is preserved.
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
