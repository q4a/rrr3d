#ifndef NX_TRIANGLE_MESH_DESC_H
#define NX_TRIANGLE_MESH_DESC_H

/*
 * PhysX 2.8's mesh descriptors -- NxSimpleTriangleMesh and the two that build
 * on it.
 *
 * Transcribed from extern/physx/include/Physics/NxTriangleMeshDesc.h and
 * NxConvexMeshDesc.h.
 *
 * These are raw pointers into caller memory with explicit strides, which is
 * exactly how the engine fills them: px::TriangleMesh points them straight at a
 * res::MeshData and cooks without copying.
 *
 * One thing the engine gets wrong that the shim must not silently correct:
 * Physx.cpp hardcodes flags = 0 when cooking, which asserts 32-bit indices. If
 * a collision mesh ever ships with 16-bit indices, 2.8 would have misread it
 * too. Reproducing the bug is the correct behaviour here; noticing it is for
 * whoever implements the cooking.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"

enum NxHeightFieldAxis
	{
	NX_X = 0,
	NX_Y = 1,
	NX_Z = 2,
	NX_NOT_HEIGHTFIELD = 0xff
	};

enum NxMeshFlags
	{
	NX_MF_FLIPNORMALS       = (1<<0),
	NX_MF_16_BIT_INDICES    = (1<<1),
	NX_MF_CONVEX            = (1<<2),
	NX_MF_HARDWARE_MESH     = (1<<3)
	};

enum NxConvexFlags
	{
	NX_CF_16_BIT_INDICES    = (1<<0),
	NX_CF_COMPUTE_CONVEX    = (1<<1),
	NX_CF_INFLATE_CONVEX    = (1<<2),
	NX_CF_USE_UNCOMPRESSED_NORMALS = (1<<3)
	};

class NxSimpleTriangleMesh
	{
	public:
	NxU32       numVertices;
	NxU32       numTriangles;
	NxU32       pointStrideBytes;
	NxU32       triangleStrideBytes;
	const void* points;
	const void* triangles;
	NxU32       flags;

	NX_INLINE NxSimpleTriangleMesh() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		numVertices         = 0;
		numTriangles        = 0;
		pointStrideBytes    = 0;
		triangleStrideBytes = 0;
		points              = NULL;
		triangles           = NULL;
		flags               = 0;
		}

	NX_INLINE bool isValid() const
		{
		if (numVertices < 3) return false;
		if (!points)         return false;
		return true;
		}
	};

class NxTriangleMeshDesc : public NxSimpleTriangleMesh
	{
	public:
	NxU32       materialIndexStride;
	const void* materialIndices;
	NxU32       heightFieldVerticalAxis;
	NxReal      heightFieldVerticalExtent;
	NxReal      convexEdgeThreshold;
	void*       pmap;

	NX_INLINE NxTriangleMeshDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		NxSimpleTriangleMesh::setToDefault();
		materialIndexStride       = 0;
		materialIndices           = 0;
		heightFieldVerticalAxis   = NX_NOT_HEIGHTFIELD;
		heightFieldVerticalExtent = 0;
		convexEdgeThreshold       = 0.001f;
		pmap                      = NULL;
		}

	NX_INLINE NxU32 checkValid() const
		{
		if (numVertices < 3) return 1;   /* at least one triangle's worth */
		/* A non-indexed mesh has to define a whole number of triangles. */
		if (!triangles && (numVertices % 3)) return 2;
		if (materialIndices && materialIndexStride < sizeof(NxMaterialIndex)) return 3;
		return 0;
		}

	NX_INLINE bool isValid() const { return !checkValid(); }
	};

class NxConvexMeshDesc
	{
	public:
	NxU32       numVertices;
	NxU32       numTriangles;
	NxU32       pointStrideBytes;
	NxU32       triangleStrideBytes;
	const void* points;
	const void* triangles;
	NxU32       flags;

	NX_INLINE NxConvexMeshDesc() { setToDefault(); }

	NX_INLINE void setToDefault()
		{
		numVertices         = 0;
		numTriangles        = 0;
		pointStrideBytes    = 0;
		triangleStrideBytes = 0;
		points              = NULL;
		triangles           = NULL;
		flags               = 0;
		}

	NX_INLINE bool isValid() const
		{
		if (numVertices < 3) return false;
		if (!points)         return false;
		/* Without NX_CF_COMPUTE_CONVEX the caller must supply the hull. */
		if (!(flags & NX_CF_COMPUTE_CONVEX) && !triangles) return false;
		return true;
		}
	};

#endif /* NX_TRIANGLE_MESH_DESC_H */
