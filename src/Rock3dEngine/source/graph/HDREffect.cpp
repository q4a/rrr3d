#include "stdafx.h"

#include "graph\\HDREffect.h"

namespace r3d
{

namespace graph
{

HDRRender::HDRRender(): _restart(true), _colorTex(0)
{
	const unsigned toneMapSz[cToneMapTexNum] = {1, 4, 16, 64};


	for (int i = 0; i < cToneMapTexNum; ++i)
	{
		Tex2DResource& tex = _toneMapTex.Add();
		tex.SetDynamic(true);
		tex.SetUsage(D3DUSAGE_RENDERTARGET);
		tex.GetOrCreateData()->SetWidth(toneMapSz[i]);
		tex.GetData()->SetHeight(toneMapSz[i]);
		tex.GetData()->SetFormat(D3DFMT_A16B16G16R16F);		

		_toneVec.push_back(&tex);
	}
}

HDRRender::~HDRRender()
{
	SetColorTex(0);

	shader.ClearTextures();
}

Tex2DResource* HDRRender::CreateRT()
{
	Tex2DResource* res = _MyBase::CreateRT();

	res->GetOrCreateData()->SetWidth(1);
	res->GetData()->SetHeight(1);
	res->GetData()->SetFormat(D3DFMT_A16B16G16R16F);

	return res;
}

void HDRRender::MeasureLuminance(Engine& engine)
{
	LSL_ASSERT(_colorTex);

	D3DXVECTOR2 sampleOffsets3x3[9];
	D3DXVECTOR2 sampleOffsets4x4[16];

	GetSampleOffsetsDownScale3x3(_colorTex->GetData()->GetWidth(), _colorTex->GetData()->GetHeight(), sampleOffsets3x3);
	shader.SetTextureDir("lumTex", _colorTex);
	shader.SetValueDir("sampleOffsets3x3", sampleOffsets3x3, sizeof(sampleOffsets3x3));
	
	IDirect3DSurface9* pSurfDest = 0;
	_toneVec[cToneMapTexNum - 1]->GetTex()->GetSurfaceLevel(0, &pSurfDest);
	engine.GetDriver().GetDevice()->SetRenderTarget(0, pSurfDest);	
	
	shader.Apply(engine, "techDown3x3LumLog", 0);
	DrawScreenQuad(engine);
	shader.UnApply(engine);
	
	pSurfDest->Release();

	for( int i = cToneMapTexNum - 1; i > 0; i--)
    {
		GetSampleOffsetsDownScale4x4(_toneVec[i - 1]->GetData()->GetWidth(), _toneVec[i - 1]->GetData()->GetHeight(), sampleOffsets4x4);		
		//Если i == 1 окончательный вариант прохода в текстуру 1х1
		shader.SetTextureDir("lumTex", _toneVec[i]);
		shader.SetValueDir("sampleOffsets4x4", sampleOffsets4x4, sizeof(sampleOffsets4x4));
		
		if (i == 1 && (engine.IsRestart() || _restart))
			GetRT()->GetTex()->GetSurfaceLevel(0, &pSurfDest);			
		else
			_toneVec[i - 1]->GetTex()->GetSurfaceLevel(0, &pSurfDest);

		engine.GetDriver().GetDevice()->SetRenderTarget(0, pSurfDest);

		shader.Apply(engine, i == 1 ? "techDown4x4LumExp" : "techDown4x4Lum", 0);
		DrawScreenQuad(engine);
		shader.UnApply(engine);

		pSurfDest->Release();
    }
}

void HDRRender::AdaptationLuminance(Engine& engine)
{
	static DWORD _oldTime = 0;
	DWORD newTime = GetTickCount();
	float dtime = (newTime - _oldTime) * 0.001f / 2.0f;
	_oldTime = newTime;

	shader.SetTextureDir("lumTex", _toneVec[0]);
	shader.SetTextureDir("lumTexOld", GetRT());
	shader.SetValueDir("dtime", dtime);
	
	ApplyRT(engine, RtFlags(0, 0));

	shader.Apply(engine, "techAdaptLum", 0);
	DrawScreenQuad(engine);
	shader.UnApply(engine);

	UnApplyRT(engine);
}

void HDRRender::Render(Engine& engine)
{
	for (_Textures::iterator iter = _toneMapTex.begin(); iter != _toneMapTex.end(); ++iter)
		(*iter)->Init(engine);

	MeasureLuminance(engine);
	//Есть необходимость первого прохода без адаптации глаза.
	if (!engine.IsRestart() && !_restart)
		AdaptationLuminance(engine);

	_restart = false;
}

Tex2DResource* HDRRender::GetColorTex()
{
	return _colorTex;
}

void HDRRender::SetColorTex(Tex2DResource* value)
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

}

}