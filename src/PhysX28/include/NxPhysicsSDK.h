#ifndef NX_PHYSICS_SDK_H
#define NX_PHYSICS_SDK_H

/*
 * PhysX 2.8's NxPhysicsSDK, its foundation, and the free functions that create
 * them.
 *
 * Transcribed from extern/physx/include/Physics/NxPhysicsSDK.h and
 * PhysXLoader/NxPhysicsSDK.h.
 *
 * NX_PHYSICS_SDK_VERSION is checked by NxCreatePhysicsSDK. The game passes it,
 * so it has to exist and to match what the shim expects -- it is 2.8.4's, and
 * the shim accepts only that.
 *
 * NX_SKIN_WIDTH is a *global* parameter here, set once to 0.025 by
 * Manager::InitSDK, and shapes inherit it by leaving their own skinWidth at -1.
 * Modern engines have no global equivalent; whatever implements this will have
 * to push it down to each shape at creation.
 */

#include "Nxp.h"
#include "NxSimpleTypes.h"
#include "NxSceneDesc.h"

class NxScene;
class NxTriangleMesh;
class NxConvexMesh;
class NxStream;

#define NX_PHYSICS_SDK_VERSION ((2 << 24) + (8 << 16) + (4 << 8) + 0)

enum NxSDKCreateError
	{
	NXCE_NO_ERROR             = 0,
	NXCE_PHYSX_NOT_FOUND      = 1,
	NXCE_WRONG_VERSION        = 2,
	NXCE_DESCRIPTOR_INVALID   = 3,
	NXCE_CONNECTION_ERROR     = 4,
	NXCE_RESET_ERROR          = 5,
	NXCE_IN_USE_ERROR         = 6,
	NXCE_BUNDLE_ERROR         = 7
	};

/* The remote debugger the engine connects to under _DEBUG. */
#define NX_DBG_DEFAULT_PORT           5425
#define NX_DBG_EVENTMASK_EVERYTHING   0xffffffff

class NxRemoteDebugger
	{
	public:
	virtual bool isConnected() const = 0;
	virtual void connect(const char* host, NxU32 port = NX_DBG_DEFAULT_PORT,
	                     NxU32 eventMask = NX_DBG_EVENTMASK_EVERYTHING) = 0;
	virtual void disconnect() = 0;
	protected:
	virtual ~NxRemoteDebugger() {}
	};

class NxFoundationSDK
	{
	public:
	virtual NxRemoteDebugger* getRemoteDebugger() = 0;
	protected:
	virtual ~NxFoundationSDK() {}
	};

class NxPhysicsSDKDesc
	{
	public:
	NxU32 flags;
	NxU32 hwPageSize;
	NxU32 hwPageMax;
	NxU32 hwConvexMax;

	NX_INLINE NxPhysicsSDKDesc() { setToDefault(); }
	NX_INLINE void setToDefault()
		{
		flags = 0;
		hwPageSize = 65536;
		hwPageMax = 256;
		hwConvexMax = 2048;
		}
	NX_INLINE bool isValid() const { return true; }
	};

class NxPhysicsSDK
	{
	public:
	virtual void release() = 0;

	virtual NxScene* createScene(const NxSceneDesc& desc) = 0;
	virtual void     releaseScene(NxScene& scene) = 0;
	virtual NxU32    getNbScenes() const = 0;

	/** NX_SKIN_WIDTH and NX_ADAPTIVE_FORCE are the two the game sets. */
	virtual bool   setParameter(NxParameter param, NxReal value) = 0;
	virtual NxReal getParameter(NxParameter param) const = 0;

	virtual NxTriangleMesh* createTriangleMesh(NxStream& stream) = 0;
	virtual void            releaseTriangleMesh(NxTriangleMesh& mesh) = 0;
	virtual NxConvexMesh*   createConvexMesh(NxStream& stream) = 0;
	virtual void            releaseConvexMesh(NxConvexMesh& mesh) = 0;

	virtual NxFoundationSDK& getFoundationSDK() const = 0;

	protected:
	virtual ~NxPhysicsSDK() {}
	};

NxPhysicsSDK* NxCreatePhysicsSDK(NxU32 sdkVersion,
                                 void* allocator = NULL,
                                 void* outputStream = NULL,
                                 const NxPhysicsSDKDesc& desc = NxPhysicsSDKDesc(),
                                 NxSDKCreateError* errorCode = NULL);

void NxReleasePhysicsSDK(NxPhysicsSDK* sdk);

#endif /* NX_PHYSICS_SDK_H */
