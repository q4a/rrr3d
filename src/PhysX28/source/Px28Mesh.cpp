/*
 * Triangle meshes, cooking, and the SDK.
 *
 * Cooking is nearly free here, and the reason is worth stating: Physx.cpp:470
 * writes a cooked mesh to a MemoryWriteBuffer and reads it straight back
 * through a MemoryReadBuffer in the next statement. Nothing is ever persisted,
 * and UserStream -- the FILE*-backed NxStream -- is declared but never called.
 * So the "cooked" format is whatever the shim likes, provided cook and create
 * agree, and no backend needs a mesh serialisation format at all.
 *
 * The format chosen is the plainest one that survives a round trip: a magic
 * word, the counts, then the de-strided vertices and indices. De-striding at
 * cook time is deliberate -- the descriptor points into engine-owned memory
 * with an arbitrary stride, and that memory is freed by FreeMesh immediately
 * after the cook returns.
 */

#include "Px28Impl.h"

namespace px28
{

namespace
{
	/* Distinguishes a stream this shim wrote from anything else that might be
	   handed to createTriangleMesh -- a saved file from the real SDK, say. */
	const NxU32 cMeshMagic = 0x50583238;   /* "PX28" */
	const NxU32 cConvexMagic = 0x50583243; /* "PX2C" */

	/* The remote debugger the engine connects to under _DEBUG. There is nothing
	   to connect to, and Physx.cpp:295 guards on isConnected() before doing
	   anything, so reporting "not connected" is the whole implementation. */
	class RemoteDebugger: public NxRemoteDebugger
		{
		public:
		virtual bool isConnected() const { return false; }
		virtual void connect(const char*, NxU32, NxU32) {}
		virtual void disconnect() {}
		};

	class Foundation: public NxFoundationSDK
		{
		public:
		virtual NxRemoteDebugger* getRemoteDebugger() { return &_debugger; }

		private:
		RemoteDebugger _debugger;
		};
}

/* ---------------------------------------------------------- convex meshes */

class ConvexMesh: public NxConvexMesh
	{
	public:
	explicit ConvexMesh(const std::vector<float>& vertices)
		: _vertices(vertices), _shape(NULL)
		{
		if (_vertices.size() < 9)
			return;

		/* Bullet hulls the point cloud itself, so the cooked form is just the
		   points. */
		_shape = new btConvexHullShape(&_vertices[0],
		                               static_cast<int>(_vertices.size() / 3),
		                               3 * sizeof(float));
		_shape->optimizeConvexHull();
		}

	virtual ~ConvexMesh() { delete _shape; }

	virtual NxU32 getCount(NxU32) const
		{
		return static_cast<NxU32>(_vertices.size() / 3);
		}

	btConvexHullShape* shape() const { return _shape; }

	private:
	std::vector<float> _vertices;
	btConvexHullShape* _shape;
	};

/* ------------------------------------------------------------ cooked mesh */

TriangleMesh::TriangleMesh(const std::vector<float>& vertices,
                           const std::vector<int>& indices)
	: _vertices(vertices), _indices(indices), _array(NULL), _shape(NULL)
	{
	if (_indices.empty() || _vertices.empty())
		return;

	_array = new btTriangleIndexVertexArray(
		static_cast<int>(_indices.size() / 3), &_indices[0], 3 * sizeof(int),
		static_cast<int>(_vertices.size() / 3), &_vertices[0], 3 * sizeof(float));

	/* A BVH, because the track is the largest static mesh in the game and every
	   raycast in Player::ResetCar walks it. */
	_shape = new btBvhTriangleMeshShape(_array, true);
	}

TriangleMesh::~TriangleMesh()
	{
	delete _shape;
	delete _array;
	}

NxU32 TriangleMesh::getCount(NxU32, NxU32) const
	{
	return static_cast<NxU32>(_indices.size() / 3);
	}

bool TriangleMesh::getTriangleVertices(NxU32 triangleIndex, NxVec3 vertices[3]) const
	{
	if (triangleIndex * 3 + 2 >= _indices.size())
		return false;

	for (int corner = 0; corner < 3; ++corner)
		{
		const int index = _indices[triangleIndex * 3 + corner];
		if (index < 0 || static_cast<size_t>(index) * 3 + 2 >= _vertices.size())
			return false;

		vertices[corner].set(_vertices[index * 3 + 0],
		                     _vertices[index * 3 + 1],
		                     _vertices[index * 3 + 2]);
		}

	return true;
	}

/* ---------------------------------------------------------------- cooking */

class Cooking: public NxCookingInterface
	{
	public:
	virtual bool NxInitCooking(void*, void*) { return true; }
	virtual void NxCloseCooking() {}

	virtual bool NxCookTriangleMesh(const NxTriangleMeshDesc& desc, NxStream& stream)
		{
		if (!desc.isValid())
			return false;

		stream.storeDword(cMeshMagic);
		stream.storeDword(desc.numVertices);
		stream.storeDword(desc.numTriangles);

		/*
		 * De-strided on the way out. The descriptor's stride is whatever the
		 * engine's vertex layout happens to be -- LoadMesh points it at live
		 * mesh data -- and FreeMesh releases that memory as soon as the cook
		 * returns, so nothing may be kept by reference.
		 */
		const char* points = static_cast<const char*>(desc.points);
		for (NxU32 i = 0; i < desc.numVertices; ++i)
			{
			const float* vertex =
				reinterpret_cast<const float*>(points + i * desc.pointStrideBytes);
			stream.storeFloat(vertex[0]);
			stream.storeFloat(vertex[1]);
			stream.storeFloat(vertex[2]);
			}

		const char* triangles = static_cast<const char*>(desc.triangles);
		const bool sixteenBit = (desc.flags & NX_MF_16_BIT_INDICES) != 0;

		for (NxU32 i = 0; i < desc.numTriangles; ++i)
			{
			const void* triangle = triangles + i * desc.triangleStrideBytes;

			for (int corner = 0; corner < 3; ++corner)
				{
				const NxU32 index = sixteenBit
					? static_cast<const NxU16*>(triangle)[corner]
					: static_cast<const NxU32*>(triangle)[corner];
				stream.storeDword(index);
				}
			}

		return true;
		}

	/*
	 * A convex hull is the same de-strided vertex dump minus the indices --
	 * Bullet computes the hull itself from a point cloud, so nothing has to be
	 * hulled at cook time.
	 *
	 * TriangleMesh::GetOrCreateConvex cooks these, but nothing in the game ever
	 * builds an NxConvexShapeDesc from one, so they are created and released
	 * and never made into a shape. Implemented anyway, because the cook and the
	 * release are both on live paths.
	 */
	virtual bool NxCookConvexMesh(const NxConvexMeshDesc& desc, NxStream& stream)
		{
		stream.storeDword(cConvexMagic);
		stream.storeDword(desc.numVertices);

		const char* points = static_cast<const char*>(desc.points);
		for (NxU32 i = 0; i < desc.numVertices; ++i)
			{
			const float* vertex =
				reinterpret_cast<const float*>(points + i * desc.pointStrideBytes);
			stream.storeFloat(vertex[0]);
			stream.storeFloat(vertex[1]);
			stream.storeFloat(vertex[2]);
			}

		return true;
		}
	};

/* --------------------------------------------------------------------- SDK */

class PhysicsSDK: public NxPhysicsSDK
	{
	public:
	PhysicsSDK(): _skinWidth(0.025f) {}

	virtual void release() {}

	virtual NxScene* createScene(const NxSceneDesc& desc)
		{
		Scene* scene = new Scene(desc);
		_scenes.push_back(scene);

		return scene;
		}

	virtual void releaseScene(NxScene& scene)
		{
		for (size_t i = 0; i < _scenes.size(); ++i)
			if (_scenes[i] == &scene)
				{
				_scenes.erase(_scenes.begin() + i);
				delete static_cast<Scene*>(&scene);
				return;
				}
		}

	virtual NxU32 getNbScenes() const { return static_cast<NxU32>(_scenes.size()); }

	/*
	 * NX_SKIN_WIDTH is a *global* parameter in 2.8, set once by
	 * Manager::InitSDK, which shapes inherit by leaving their own at -1. It is
	 * stored rather than pushed anywhere yet -- see the note in NxShapeDesc.h
	 * about the 69 shapes in db.xml that override it.
	 */
	virtual bool setParameter(NxParameter param, NxReal value)
		{
		if (param == NX_SKIN_WIDTH)
			{
			_skinWidth = value;
			return true;
			}

		/* NX_ADAPTIVE_FORCE is the other one the game sets; accepting and
		   ignoring it is honest, because Bullet has no equivalent knob and the
		   default 2.8 behaviour is what this reproduces. */
		return true;
		}

	virtual NxReal getParameter(NxParameter param) const
		{
		return param == NX_SKIN_WIDTH ? _skinWidth : 0.0f;
		}

	virtual NxTriangleMesh* createTriangleMesh(NxStream& stream)
		{
		if (stream.readDword() != cMeshMagic)
			return NULL;

		const NxU32 numVertices = stream.readDword();
		const NxU32 numTriangles = stream.readDword();

		std::vector<float> vertices(numVertices * 3);
		for (NxU32 i = 0; i < numVertices * 3; ++i)
			vertices[i] = stream.readFloat();

		std::vector<int> indices(numTriangles * 3);
		for (NxU32 i = 0; i < numTriangles * 3; ++i)
			indices[i] = static_cast<int>(stream.readDword());

		TriangleMesh* mesh = new TriangleMesh(vertices, indices);
		_meshes.push_back(mesh);

		return mesh;
		}

	virtual void releaseTriangleMesh(NxTriangleMesh& mesh)
		{
		for (size_t i = 0; i < _meshes.size(); ++i)
			if (_meshes[i] == &mesh)
				{
				_meshes.erase(_meshes.begin() + i);
				delete static_cast<TriangleMesh*>(&mesh);
				return;
				}
		}

	virtual NxConvexMesh* createConvexMesh(NxStream& stream)
		{
		if (stream.readDword() != cConvexMagic)
			return NULL;

		const NxU32 numVertices = stream.readDword();

		std::vector<float> vertices(numVertices * 3);
		for (NxU32 i = 0; i < numVertices * 3; ++i)
			vertices[i] = stream.readFloat();

		ConvexMesh* mesh = new ConvexMesh(vertices);
		_convexMeshes.push_back(mesh);

		return mesh;
		}

	virtual void releaseConvexMesh(NxConvexMesh& mesh)
		{
		for (size_t i = 0; i < _convexMeshes.size(); ++i)
			if (_convexMeshes[i] == &mesh)
				{
				_convexMeshes.erase(_convexMeshes.begin() + i);
				delete static_cast<ConvexMesh*>(&mesh);
				return;
				}
		}

	virtual NxFoundationSDK& getFoundationSDK() const
		{
		return _foundation;
		}

	private:
	std::vector<NxScene*> _scenes;
	std::vector<NxTriangleMesh*> _meshes;
	std::vector<NxConvexMesh*> _convexMeshes;
	NxReal _skinWidth;
	mutable Foundation _foundation;
	};

} /* namespace px28 */

/* ------------------------------------------------------------ entry points */

NxPhysicsSDK* NxCreatePhysicsSDK(NxU32 sdkVersion, void*, void*,
                                 const NxPhysicsSDKDesc&, NxSDKCreateError* errorCode)
	{
	/* The version check is 2.8's own and the game passes NX_PHYSICS_SDK_VERSION,
	   so a mismatch means the game was built against a header this shim does not
	   implement -- which is worth refusing rather than half-serving. */
	if (sdkVersion != NX_PHYSICS_SDK_VERSION)
		{
		if (errorCode)
			*errorCode = NXCE_WRONG_VERSION;
		return NULL;
		}

	if (errorCode)
		*errorCode = NXCE_NO_ERROR;

	return new px28::PhysicsSDK();
	}

void NxReleasePhysicsSDK(NxPhysicsSDK* sdk)
	{
	delete static_cast<px28::PhysicsSDK*>(sdk);
	}

NxCookingInterface* NxGetCookingLib(NxU32 sdkVersionNumber)
	{
	if (sdkVersionNumber != NX_PHYSICS_SDK_VERSION)
		return NULL;

	/* 2.8 hands back a singleton owned by the cooking library, not something
	   the caller releases -- Manager::InitSDK keeps the pointer for the process
	   lifetime and never frees it. */
	static px28::Cooking cooking;

	return &cooking;
	}
