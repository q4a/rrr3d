#ifndef HDR_RENDER
#define HDR_RENDER

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class ToneMapping: public PostEffRender<Tex2DResource>
{
private:
	typedef PostEffRender<Tex2DResource> _MyBase;
protected:
	virtual Tex2DResource* CreateRT();
public:
	virtual void Render(Engine& engine);
	
	Tex2DResource* GetColorTex();
	void SetColorTex(Tex2DResource* value);	
	Tex2DResource* GetBloomTex();
	void SetBloomTex(Tex2DResource* value);
	Tex2DResource* GetHDRTex();
	void SetHDRTex(Tex2DResource* value);

	Shader shader;
};

}

}

#endif