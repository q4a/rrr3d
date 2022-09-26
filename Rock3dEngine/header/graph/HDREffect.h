#ifndef HDR_EFFECT
#define HDR_EFFECT

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class HDRRender: public PostEffRender<Tex2DResource>
{
private:
	typedef PostEffRender<Tex2DResource> _MyBase;
	typedef lsl::ResourceCollection<Tex2DResource, void, void, void> _Textures;

	static const int cToneMapTexNum = 4;
private:
	Tex2DResource* _colorTex;
	bool _restart;
protected:
	virtual Tex2DResource* CreateRT();

	void MeasureLuminance(Engine& engine);
	void AdaptationLuminance(Engine& engine);
public:
	HDRRender();
	virtual ~HDRRender();

	virtual void Render(Engine& engine);
	
	Tex2DResource* GetColorTex();
	void SetColorTex(Tex2DResource* value);

	_Textures _toneMapTex;
	std::vector<Tex2DResource*> _toneVec;
	Shader shader;
};

}

}

#endif