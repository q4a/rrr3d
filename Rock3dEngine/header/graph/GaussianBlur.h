#ifndef GAUSSIAN_BLUR
#define GAUSSIAN_BLUR

#include "MaterialLibrary.h"
#include "RenderToTexture.h"

namespace r3d
{

namespace graph
{

class GaussianBlurShader: public D3DXEffect
{
public:
	enum TechniqueType {ttHorizontal, ttVertical};
private:
	D3DXHANDLE _colorTex;
	D3DXHANDLE _colorTexSizes;
	D3DXHANDLE _techGaussianBlur;
protected:
	virtual void DoInit();
	virtual void DoUpdate();
	virtual void DoApplyTech(Engine& engine);
public:
	GaussianBlurShader();

	TechniqueType curTechnique;
};

class GaussianBlurRender: public RenderTargetTex
{
private:
	GaussianBlurShader* _shader;
	Tex2DResource* _colorTex;
protected:
	virtual void DoCreateRT(Tex2DResource* rt);
public:
	GaussianBlurRender();
	virtual ~GaussianBlurRender();

	virtual void Render(Engine& engine, IDirect3DSurface9* backBuffer, IDirect3DSurface9* dsSurface);

	Tex2DResource* GetColorTex();
	void SetColorTex(Tex2DResource* value);
	GaussianBlurShader* GetShader();
	void SetShader(GaussianBlurShader* value);
};

}

}

#endif