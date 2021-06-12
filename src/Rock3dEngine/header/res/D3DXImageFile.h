#include "GraphResource.h"
#include "graph\\driver\\RenderDriver.h"

namespace r3d
{

namespace res
{
 
class D3DXImageFile: public lsl::IOResource<ImageResource>
{
public:
	static void RegistredFile();

	static graph::RenderDriver* driver;
public:	
	virtual void LoadFromStream(Resource& outData, std::istream& stream);
	virtual void SaveToStream(const Resource& inData, std::ostream& stream);
};

class D3DXCubeImageFile: public lsl::IOResource<CubeImageResource>
{
public:
	static void RegistredFile();

	static graph::RenderDriver* driver;
public:	
	virtual void LoadFromStream(Resource& outData, std::istream& stream);
	virtual void SaveToStream(const Resource& inData, std::ostream& stream);
};

}

}