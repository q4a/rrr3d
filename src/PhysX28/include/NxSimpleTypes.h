#ifndef NX_SIMPLE_TYPES_H
#define NX_SIMPLE_TYPES_H

/*
 * The PhysX 2.8 scalar types.
 *
 * Transcribed from extern/physx/include/Foundation/NxSimpleTypes.h and Nxf.h.
 * NxReal is NxF32 unless NX_USE_FLOAT is off; this game builds single-precision
 * and every stored value in db.xml is a float, so single is what is provided.
 * The double branch is not carried, because carrying an untested one is worse
 * than not having it.
 */

#include <cfloat>
#include <cstddef>

typedef signed int      NxI32;
typedef signed short    NxI16;
typedef signed char     NxI8;
typedef unsigned int    NxU32;
typedef unsigned short  NxU16;
typedef unsigned char   NxU8;
typedef float           NxF32;
typedef double          NxF64;

typedef NxF32           NxReal;

#define NX_MAX_F32      FLT_MAX
#define NX_MIN_F32      (-FLT_MAX)
#define NX_MAX_REAL     NX_MAX_F32
#define NX_MIN_REAL     NX_MIN_F32
#define NX_EPS_F32      FLT_EPSILON
#define NX_EPS_REAL     NX_EPS_F32

/*
 * NX_INLINE is the SDK's spelling and appears in the game's own px/Stream.h
 * overrides, so it has to exist under that name.
 */
#ifndef NX_INLINE
#define NX_INLINE inline
#endif

/*
 * NX_ASSERT and NX_DELETE_ARRAY are used by px/Stream.h. The SDK routes the
 * first through its own error stream; here it is the platform assert, which is
 * the same contract from the caller's side.
 */
#ifndef NX_ASSERT
#include <cassert>
#define NX_ASSERT(x) assert(x)
#endif

#ifndef NX_DELETE_ARRAY
#define NX_DELETE_ARRAY(x) do { delete [] (x); (x) = 0; } while (0)
#endif

#ifndef NX_DELETE
#define NX_DELETE(x) do { delete (x); (x) = 0; } while (0)
#endif

#endif /* NX_SIMPLE_TYPES_H */
