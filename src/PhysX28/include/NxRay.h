#ifndef NX_RAY_H
#define NX_RAY_H

/*
 * PhysX 2.8's NxRay, NxTriangle and NxBounds3.
 *
 * Transcribed from extern/physx/include/Foundation/. Small enough to share a
 * header; the game never includes any of them by name.
 *
 * One thing the shim must NOT fix: NxRay does not require its direction to be
 * normalised, and two call sites in Weapon.cpp pass an unnormalised one. Every
 * modern engine's raycast does require it. Normalising inside the shim would be
 * the right thing for a new codebase and the wrong thing here -- it would
 * change the meaning of maxDist at those two sites, quietly shortening or
 * lengthening the ray. Whatever implements raycasting has to normalise for the
 * backend *and* scale the distance to match.
 */

#include "NxSimpleTypes.h"
#include "NxVec3.h"

class NxRay
	{
	public:
	NxVec3 orig;
	/** Not required to be normalised. See the note above. */
	NxVec3 dir;

	NX_INLINE NxRay() {}
	NX_INLINE NxRay(const NxVec3 &origin, const NxVec3 &direction)
		: orig(origin), dir(direction) {}
	};

class NxTriangle
	{
	public:
	NxVec3 verts[3];

	NX_INLINE NxTriangle() {}
	NX_INLINE NxTriangle(const NxVec3 &a, const NxVec3 &b, const NxVec3 &c)
		{ verts[0] = a; verts[1] = b; verts[2] = c; }

	/* Computes into the argument, matching 2.8's style elsewhere. */
	NX_INLINE void normal(NxVec3 &out) const
		{
		out.cross(verts[1] - verts[0], verts[2] - verts[0]);
		out.normalize();
		}

	NX_INLINE void denormalizedNormal(NxVec3 &out) const
		{ out.cross(verts[1] - verts[0], verts[2] - verts[0]); }
	};

class NxBounds3
	{
	public:
	NxVec3 min, max;

	NX_INLINE NxBounds3() { setEmpty(); }

	NX_INLINE void setEmpty()
		{
		min.set(NX_MAX_REAL, NX_MAX_REAL, NX_MAX_REAL);
		max.set(NX_MIN_REAL, NX_MIN_REAL, NX_MIN_REAL);
		}
	NX_INLINE void set(const NxVec3 &lo, const NxVec3 &hi) { min = lo; max = hi; }
	NX_INLINE void include(const NxVec3 &v) { min.min(v); max.max(v); }
	NX_INLINE bool isEmpty() const { return min.x > max.x; }
	NX_INLINE void getCenter(NxVec3 &out) const { out = (min + max) * NxReal(0.5); }
	NX_INLINE void getExtents(NxVec3 &out) const { out = (max - min) * NxReal(0.5); }
	NX_INLINE bool contain(const NxVec3 &v) const
		{
		return v.x >= min.x && v.x <= max.x
		    && v.y >= min.y && v.y <= max.y
		    && v.z >= min.z && v.z <= max.z;
		}
	};

class NxShape;
class NxMaterial;

class NxRaycastHit
	{
	public:
	NxShape* shape;
	NxVec3   worldImpact;
	NxVec3   worldNormal;
	NxReal   distance;
	NxU32    faceID;
	NxU32    internalFaceID;
	NxReal   u, v;
	NxMaterial* material;

	NX_INLINE NxRaycastHit()
		: shape(NULL), distance(0), faceID(0), internalFaceID(0),
		  u(0), v(0), material(NULL) {}
	};

#endif /* NX_RAY_H */
