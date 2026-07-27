/*
 * The Win32 window-system driver, supplied here rather than by DXVK.
 *
 * DXVK picks a driver from a table of WsiBootstrap entries, each guarded by a
 * DXVK_WSI_* define. d9mt installs itself by subclassing Win32WsiDriver and
 * repointing Win32WSI::createDriver at its own factory -- a deliberate trick,
 * noted at that call site, that avoids editing the vendored tree. It calls
 * through to the base for display-mode queries.
 *
 * Both the base class and the table entry normally come from
 * wsi/win32/wsi_platform_win32.cpp, which is Win32 throughout: EnumDisplay
 * Settings, ChangeDisplaySettings, monitor enumeration. Compiling that would
 * mean substituting the whole Win32 display API for a build that, right now,
 * has no display server behind HWND at all.
 *
 * So the base is implemented here for a display-less build instead: one
 * monitor at a fixed desktop mode, no mode switching, no fullscreen. That is
 * honest about what exists -- there is no window yet -- and it is a small,
 * obvious file to replace once the SDL shell lands, at which point most of
 * these become SDL_GetDisplayMode and friends.
 */

#include "../dxvk/src/wsi/wsi_platform.h"
#include "../dxvk/src/wsi/win32/wsi_platform_win32.h"
#include "../dxvk/src/dxvk/dxvk_cmdlist.h"

#include <cstring>

namespace dxvk::wsi {

  namespace {

    /*
     * The mode reported for every query. The engine asks for the desktop mode
     * while choosing a back buffer, so this has to be plausible rather than
     * zeroed -- a 0x0 desktop makes the caller pick nothing.
     */
    constexpr uint32_t kDefaultWidth  = 1280;
    constexpr uint32_t kDefaultHeight = 720;

    /* Non-null placeholder: callers test monitors for null, never dereference. */
    HMONITOR DefaultMonitor() {
      return reinterpret_cast<HMONITOR>(1);
    }

    void FillDefaultMode(WsiMode* pMode) {
      if (!pMode)
        return;

      pMode->width                   = kDefaultWidth;
      pMode->height                  = kDefaultHeight;
      pMode->refreshRate.numerator   = 60000;
      pMode->refreshRate.denominator = 1000;
      pMode->bitsPerPixel            = 32;
      pMode->interlaced              = false;
    }

  }

  std::vector<const char*> Win32WsiDriver::getInstanceExtensions() {
    /* No Vulkan instance; the backend is Metal. */
    return {};
  }

  HMONITOR Win32WsiDriver::getDefaultMonitor() {
    return DefaultMonitor();
  }

  HMONITOR Win32WsiDriver::enumMonitors(uint32_t index) {
    return index == 0 ? DefaultMonitor() : nullptr;
  }

  HMONITOR Win32WsiDriver::enumMonitors(const LUID* [], uint32_t, uint32_t index) {
    return enumMonitors(index);
  }

  bool Win32WsiDriver::getDisplayName(HMONITOR hMonitor, WCHAR (&Name)[32]) {
    if (!hMonitor)
      return false;

    const wchar_t name[] = L"Metal";
    std::memcpy(Name, name, sizeof(name));
    return true;
  }

  bool Win32WsiDriver::getDesktopCoordinates(HMONITOR hMonitor, RECT* pRect) {
    if (!hMonitor || !pRect)
      return false;

    pRect->left   = 0;
    pRect->top    = 0;
    pRect->right  = kDefaultWidth;
    pRect->bottom = kDefaultHeight;
    return true;
  }

  bool Win32WsiDriver::getDisplayMode(HMONITOR hMonitor, uint32_t modeNumber, WsiMode* pMode) {
    /* Exactly one mode, so enumeration terminates after the first. */
    if (!hMonitor || modeNumber != 0)
      return false;

    FillDefaultMode(pMode);
    return true;
  }

  bool Win32WsiDriver::getCurrentDisplayMode(HMONITOR hMonitor, WsiMode* pMode) {
    if (!hMonitor)
      return false;

    FillDefaultMode(pMode);
    return true;
  }

  bool Win32WsiDriver::getDesktopDisplayMode(HMONITOR hMonitor, WsiMode* pMode) {
    return getCurrentDisplayMode(hMonitor, pMode);
  }

  WsiEdidData Win32WsiDriver::getMonitorEdid(HMONITOR) {
    /* No EDID without a display; callers treat empty as "unknown". */
    return {};
  }

  void Win32WsiDriver::getWindowSize(HWND, uint32_t* pWidth, uint32_t* pHeight) {
    if (pWidth)
      *pWidth = kDefaultWidth;
    if (pHeight)
      *pHeight = kDefaultHeight;
  }

  void Win32WsiDriver::resizeWindow(HWND, DxvkWindowState*, uint32_t, uint32_t) {
  }

  bool Win32WsiDriver::setWindowMode(HMONITOR hMonitor, HWND, DxvkWindowState*, const WsiMode&) {
    /* Succeeds without switching: there is no mode to switch. */
    return hMonitor != nullptr;
  }

  bool Win32WsiDriver::enterFullscreenMode(HMONITOR hMonitor, HWND, DxvkWindowState*, bool) {
    return hMonitor != nullptr;
  }

  bool Win32WsiDriver::leaveFullscreenMode(HWND, DxvkWindowState*, bool) {
    return true;
  }

  bool Win32WsiDriver::restoreDisplayMode() {
    return true;
  }

  HMONITOR Win32WsiDriver::getWindowMonitor(HWND) {
    return DefaultMonitor();
  }

  bool Win32WsiDriver::isWindow(HWND) {
    /*
     * True regardless of the handle. The engine passes whatever it was given,
     * which is null until the SDL shell exists, and answering false would make
     * the swapchain refuse to initialise before anything has had a chance to
     * render.
     */
    return true;
  }

  bool Win32WsiDriver::isMinimized(HWND) {
    return false;
  }

  bool Win32WsiDriver::isOccluded(HWND) {
    return false;
  }

  void Win32WsiDriver::updateFullscreenWindow(HMONITOR, HWND, bool) {
  }

  VkResult Win32WsiDriver::createSurface(
          HWND                      hWindow,
          PFN_vkGetInstanceProcAddr,
          VkInstance,
          VkSurfaceKHR*             pSurface) {
    if (!pSurface)
      return VK_ERROR_INITIALIZATION_FAILED;

    /* The HWND is carried through as the surface handle; see d9mt_instance.cpp. */
    *pSurface = reinterpret_cast<VkSurfaceKHR>(hWindow);
    return VK_SUCCESS;
  }

  /*
   * The table entry d9mt repoints. This default never runs -- installWsiDriver()
   * overwrites createDriver before wsi::init() -- and failing loudly here beats
   * a null driver surfacing somewhere further in.
   */
  static bool createNullWsiDriver(WsiDriver** driver) {
    *driver = nullptr;
    return false;
  }

  /*
   * The name matters: wsi::init() selects a driver by comparing this against
   * DXVK_WSI_DRIVER, which defaults to "Win32" when DXVK_WSI_WIN32 is defined.
   * Anything else here and no entry matches, and init throws "Failed to
   * initialize WSI."
   */
  WsiBootstrap Win32WSI = {
    "Win32",
    createNullWsiDriver
  };

}

/*
 * DxvkCommandList::allocateCommandBuffer.
 *
 * Declared in dxvk_cmdlist.h and reached from the inline cmdDispatch and
 * cmdPipelineBarrier helpers, which d3d9_format_helpers.cpp instantiates for
 * DXVK's video-format conversion shaders (YUY2, NV12, YV12 and friends).
 *
 * Upstream defines it in dxvk_cmdlist.cpp, which is Vulkan command-pool
 * management -- exactly the layer d9mt's Metal backend replaces, and which is
 * therefore not compiled here. Nothing in the Metal path calls it: d9mt encodes
 * through MTLCommandBuffer, not VkCommandBuffer.
 *
 * So it exists to satisfy the linker for code that is reachable in principle
 * and dead in practice, and returns null. Should a video-format conversion ever
 * actually run, that null propagates into a Vulkan call that is equally absent,
 * which would be loud. This game plays .avi through the video layer, not
 * through D3D9 surface formats, so it should not arise.
 */
namespace dxvk {

  VkCommandBuffer DxvkCommandList::allocateCommandBuffer(DxvkCmdBuffer) {
    return VK_NULL_HANDLE;
  }

}
