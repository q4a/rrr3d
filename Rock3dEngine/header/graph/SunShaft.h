#ifndef SUN_SHAFT
#define SUN_SHAFT

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class SunShaftRender: public PostEffRender<Tex2DResource>
{
public:
	typedef lsl::ResourceCollection<Tex2DResource, void, void, void> Textures;
public:
	static const int cShaftsTexSizeX = 1280;
	static const int cShaftsTexSizeY = 1024;
	static const int cShaftsTexNum   = 2;
private:
	D3DXVECTOR3 _sunPos;
public:
	SunShaftRender();
	virtual ~SunShaftRender();

	virtual void Render(Engine& engine);

	Tex2DResource* GetColorTex();
	void SetColorTex(Tex2DResource* value);

	Tex2DResource* GetDepthTex();
	void SetDepthTex(Tex2DResource* value);

	const D3DXVECTOR3& GetSunPos() const;
	void SetSunPos(const D3DXVECTOR3& value);

	Shader shader;

	Textures _blurTex;
	std::vector<Tex2DResource*> _blurVec;
};

}

}

#endif