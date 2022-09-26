#include "graph\\GaussianBlur.h"

namespace r3d
{

namespace graph
{

GaussianBlurShader::GaussianBlurShader(): curTechnique(ttHorizontal)
{
}

void GaussianBlurShader::DoInit()
{
	D3DXEffect::DoInit();

	_colorTex = GetEffect()->GetParameterByName(0, "colorTex");
	_colorTexSizes = GetEffect()->GetParameterByName(0, "colorTexSizes");
	_techGaussianBlur = GetEffect()->GetTechniqueByName("techGaussianBlur");	
}

void GaussianBlurShader::DoUpdate()
{
	//Nothing
}

void GaussianBlurShader::DoApplyTech(Engine& engine)
{
	if (!engine.GetTexture(0) || engine.GetTexture(0)->GetType() != D3DRTYPE_TEXTURE)
		throw int();
	D3DSURFACE_DESC texDesc;
	static_cast<IDirect3DTexture9*>(engine.GetTexture(0))->GetLevelDesc(0, &texDesc);

	float fWidth = 0;
	float fHeight = 0;
	switch (curTechnique)
	{
	case ttHorizontal:
		fWidth = 1.0f / texDesc.Width;
		break;

	case ttVertical:
		fHeight = 1.0f / texDesc.Height;
		break;

	default:
		throw int();
	}

	GetEffect()->SetTexture(_colorTex, engine.GetTexture(0));
	GetEffect()->SetFloatArray(_colorTexSizes, D3DXVECTOR2(fWidth, fHeight), 2);
	GetEffect()->SetTechnique(_techGaussianBlur);
	GetEffect()->Begin(&_cntPass, 0);
}




GaussianBlurRender::GaussianBlurRender(): _shader(0), _colorTex(0)
{	
}

GaussianBlurRender::~GaussianBlurRender()
{
	SetColorTex(0);
	SetShader(0);
}

void GaussianBlurRender::DoCreateRT(Tex2DResource* rt)
{
	rt->GetData()->SetFormat(D3DFMT_X8R8G8B8);
}

void GaussianBlurRender::Render(Engine& engine, IDirect3DSurface9* backBuffer, IDirect3DSurface9* dsSurface)
{
	if (!_shader || !GetRT() || !GetRT()->IsInit() || !_colorTex)
		throw int();

	_shader->Apply(engine);
	IDirect3DSurface9* targetSurf;
	GetRT()->GetTex()->GetSurfaceLevel(0, &targetSurf);
	GetDriver().GetDevice()->SetRenderTarget(0, targetSurf);
	engine.SetTexture(0, _colorTex->GetTex());

	//Horizontal
	_shader->curTechnique = GaussianBlurShader::ttHorizontal;
	engine.BeginDraw();
	DrawScreenQuad(engine);
	engine.EndDraw();

	//Vertical
	_shader->curTechnique = GaussianBlurShader::ttVertical;
	engine.BeginDraw();
	DrawScreenQuad(engine);
	engine.EndDraw();

	_shader->UnApply(engine);
	engine.SetTexture(0, 0);	
}

Tex2DResource* GaussianBlurRender::GetColorTex()
{
	return _colorTex;
}

void GaussianBlurRender::SetColorTex(Tex2DResource* value)
{
	if (_colorTex != value)
	{
		if (_colorTex)				
			_colorTex->Release();
		_colorTex = value;
		if (_colorTex)		
			_colorTex->AddRef();
	}
}

GaussianBlurShader* GaussianBlurRender::GetShader()
{
	return _shader;
}

void GaussianBlurRender::SetShader(GaussianBlurShader* value)
{
	if (_shader != value)
	{
		if (_shader)				
			_shader->Release();
		_shader = value;
		if (_shader)		
			_shader->AddRef();
	}
}

}

}