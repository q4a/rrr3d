#ifndef R3D_GRAPH_CLEARSURF
#define R3D_GRAPH_CLEARSURF

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class ClearSurf: public PostEffRender<Tex2DResource>
{
public:
	enum Mode {cmColor, cmMaxDepth};
private:
	Mode _mode;
	D3DXCOLOR _color;

	void ApplyMode();
public:
	ClearSurf();

	virtual void Render(Engine& engine);

	Mode GetMode() const;
	void SetMode(Mode value);

	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	Shader shader;
};

}

}

#endif