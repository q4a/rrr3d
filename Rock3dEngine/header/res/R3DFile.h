#ifndef R3D_MESH_FILE
#define R3D_MESH_FILE

#include "GraphResource.h"

namespace r3d
{

namespace res
{

class R3DMeshFile: public lsl::IOResource<MeshData>
{
public:
	static void RegistredFile();
public:
	virtual void LoadFromStream(Resource& outData, std::istream& stream);
	virtual void SaveToStream(const Resource& inData, std::ostream& stream);
};

}

}

#endif