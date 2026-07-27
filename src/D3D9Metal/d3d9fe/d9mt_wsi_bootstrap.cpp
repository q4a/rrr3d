/*
 * The Win32WSI bootstrap entry, supplied here rather than by DXVK.
 *
 * DXVK picks a window-system driver from a table of WsiBootstrap entries, each
 * guarded by a DXVK_WSI_* define. d9mt_wsi.cpp installs its own driver by
 * repointing Win32WSI::createDriver at its factory -- a deliberate trick, noted
 * at that call site, that avoids editing the vendored tree.
 *
 * That needs the symbol to exist. On Windows it comes from
 * wsi/win32/wsi_platform_win32.cpp, which is Win32 through and through and has
 * no business here. So the entry is defined below instead: DXVK sees a table
 * slot, d9mt overwrites the factory before wsi::init() runs, and the default
 * never fires.
 *
 * If it ever does fire -- meaning installWsiDriver() was not called first --
 * failing is the right answer, loudly and here, rather than a null driver
 * surfacing somewhere further in.
 */

#include "../dxvk/src/wsi/wsi_platform.h"

namespace dxvk::wsi {

  static bool createNullWsiDriver(WsiDriver** driver) {
    *driver = nullptr;
    return false;
  }

  WsiBootstrap Win32WSI = {
    "D9MT",
    createNullWsiDriver
  };

}
