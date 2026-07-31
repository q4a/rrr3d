// The shim's enumerations are header-only, so this exists to give the library a
// translation unit and to check the header parses standalone -- which matters,
// because Nxp.h is included by every consumer and must not depend on include
// order to compile.

#include "Nxp.h"

namespace
{

// Values the shipped data serialises, pinned at compile time as well as in the
// test binary. If one of these ever changes, this file stops compiling rather
// than the game quietly loading a track with different friction.
static_assert(NX_WF_CLAMPED_FRICTION == 64, "db.xml stores wheelFlags 64");
static_assert((NX_AF_LOCK_COM | NX_AF_CONTACT_MODIFICATION) == 20, "db.xml stores flags 20");
static_assert((NX_BF_VISUALIZATION | NX_BF_ENERGY_SLEEP_TEST) == 2304, "db.xml stores flags 2304");

static_assert(NX_SHAPE_WHEEL == 4, "NxShapeType is positional");
static_assert(NX_ACCELERATION == 5, "NxForceMode is positional");
static_assert(NX_NOTIFY_ALL == 0x7fe, "NX_NOTIFY_ALL is ten flags");
static_assert((NX_NOTIFY_ALL & NX_NOTIFY_CONTACT_MODIFICATION) == 0,
              "the SDK excludes contact modification from NX_NOTIFY_ALL");

} // namespace
