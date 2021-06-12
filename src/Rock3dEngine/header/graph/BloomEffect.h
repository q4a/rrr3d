#ifndef BLOOM_EFFECT
#define BLOOM_EFFECT

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class BloomRender: public PostEffRender<Tex2DResource>
{
private:
	typedef PostEffRender<Tex2DResource> _MyBase;
private:
	Tex2DResource _bloomTex;
	Tex2DResource* _colorTex;
protected:
	virtual Tex2DResource* CreateRT();
public:
	BloomRender();
	virtual ~BloomRender();

	virtual void Render(Engine& engine);

	Tex2DResource* GetColorTex();
	void SetColorTex(Tex2DResource* value);
	Tex2DResource* GetLumTex();
	void SetLumTex(Tex2DResource* value);

	Shader shader;
};

}

}

#endif