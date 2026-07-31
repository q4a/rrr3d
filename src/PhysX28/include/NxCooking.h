#ifndef NX_COOKING_H
#define NX_COOKING_H

/*
 * PhysX 2.8's cooking interface.
 *
 * Transcribed from extern/physx/include/Cooking/NxCooking.h.
 *
 * Cooking here is cheaper than it looks. px::TriangleMesh cooks into a
 * MemoryWriteBuffer and immediately reads it back through a MemoryReadBuffer --
 * nothing is ever written to disk, and UserStream, the FILE*-backed variant, is
 * declared but never called. So the "cooked" format is entirely the shim's own
 * business, provided the same shim reads it back.
 *
 * That is worth stating because it removes what would otherwise be a real
 * constraint on the backend: no engine's mesh serialisation format has to be
 * matched, or even used.
 */

#include "NxSimpleTypes.h"
#include "NxTriangleMeshDesc.h"

class NxStream;

class NxCookingInterface
	{
	public:
	virtual bool NxInitCooking(void* allocator = NULL, void* outputStream = NULL) = 0;
	virtual void NxCloseCooking() = 0;
	virtual bool NxCookTriangleMesh(const NxTriangleMeshDesc& desc, NxStream& stream) = 0;
	virtual bool NxCookConvexMesh(const NxConvexMeshDesc& desc, NxStream& stream) = 0;
	protected:
	virtual ~NxCookingInterface() {}
	};

NxCookingInterface* NxGetCookingLib(NxU32 sdkVersionNumber);

#endif /* NX_COOKING_H */
