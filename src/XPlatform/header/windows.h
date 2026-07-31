#ifndef XPLATFORM_WINDOWS_H
#define XPLATFORM_WINDOWS_H

/*
 * Stands in for <windows.h> so the engine's includes need no #ifdef.
 *
 * This directory is only on the include path off Windows, so on Windows the
 * real header is found instead and this file is never seen.
 */

#include "xplatform.h"

#endif
