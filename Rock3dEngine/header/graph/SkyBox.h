#ifndef SKY_BOX_RENDER
#define SKY_BOX_RENDER

#include "MaterialLibrary.h"

namespace r3d
{

namespace graph
{

class SkyBox: public Renderable
{
public:
	enum CoordSystem {csLeft, csRight};
private:
	VBMesh _mesh;
	CoordSystem _coordSystem;
public:
	SkyBox();
	
	virtual void WorldDimensions(AABB& aabb) const;
	virtual void Render(Engine& engine);
	virtual void ProgressTime(const float newTime, const float dt);

	TexCubeResource* GetSkyTex();
	void SetSkyTex(TexCubeResource* value);
	CoordSystem GetCoordSystem() const;
	void SetCoordSystem(CoordSystem value);

	Shader shader;
};

}

}

#endif