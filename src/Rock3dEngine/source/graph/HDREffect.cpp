#include "stdafx.h"

#include "graph/HDREffect.h"

#include "rrr3d_trace.h"

namespace r3d
{

namespace graph
{

namespace
{

float HalfToFloat(unsigned short h)
{
	const unsigned sign = (h >> 15) & 0x1;
	const unsigned exponent = (h >> 10) & 0x1f;
	const unsigned mantissa = h & 0x3ff;

	unsigned bits;
	if (exponent == 0)
	{
		//Zero or subnormal. Subnormal halves are far below anything a luminance
		//reduction produces, so they are reported as zero rather than scaled.
		bits = sign << 31;
	}
	else if (exponent == 0x1f)
	{
		//Inf or NaN, which is the whole reason this function exists.
		bits = (sign << 31) | 0x7f800000u | (mantissa << 13);
	}
	else
	{
		bits = (sign << 31) | ((exponent - 15 + 127) << 23) | (mantissa << 13);
	}

	float result;
	std::memcpy(&result, &bits, sizeof(result));
	return result;
}

/*
 * DIAGNOSTIC: read one A16B16G16R16F pixel back off a render target.
 *
 * The HDR chain reduces the scene to a single average luminance and tone maps
 * by it. Every pass in that chain executes with draws in it and the result is
 * black, so the question is not whether it runs but what number comes out --
 * and a 1x1 target is cheap enough to stall on once a second.
 */
void TraceLuminanceTexel(Engine& engine, Tex2DResource* tex, const char* label)
{
	if (!::rrr3d::TraceEnabled() || !tex || !tex->GetTex())
		return;

	IDirect3DDevice9* device = engine.GetDriver().GetDevice();

	IDirect3DSurface9* source = 0;
	if (FAILED(tex->GetTex()->GetSurfaceLevel(0, &source)) || !source)
		return;

	D3DSURFACE_DESC desc;
	source->GetDesc(&desc);

	IDirect3DSurface9* copy = 0;
	if (SUCCEEDED(device->CreateOffscreenPlainSurface(desc.Width, desc.Height,
			desc.Format, D3DPOOL_SYSTEMMEM, &copy, 0)) &&
		SUCCEEDED(device->GetRenderTargetData(source, copy)))
	{
		D3DLOCKED_RECT rect;
		if (SUCCEEDED(copy->LockRect(&rect, 0, D3DLOCK_READONLY)))
		{
			const unsigned short* texel = static_cast<const unsigned short*>(rect.pBits);
			RRR3D_TRACE("LUM %s %ux%u fmt=%d rgba=%.6f %.6f %.6f %.6f",
				label, desc.Width, desc.Height, (int)desc.Format,
				HalfToFloat(texel[0]), HalfToFloat(texel[1]),
				HalfToFloat(texel[2]), HalfToFloat(texel[3]));
			copy->UnlockRect();
		}
	}

	if (copy)
		copy->Release();
	source->Release();
}

}

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

	//DIAGNOSTIC: the numbers coming out of the reduction, once every two
	//seconds. Reading a render target back stalls the pipeline, so this is
	//deliberately sparse -- the value drifts slowly by design, and one sample
	//per two seconds is enough to see whether it is sane, zero, or NaN.
	static unsigned frame = 0;
	const bool sample = (frame++ % 120) == 0;
	if (sample)
	{
		TraceLuminanceTexel(engine, _toneVec[cToneMapTexNum - 1], "down3x3(64x64)");
		TraceLuminanceTexel(engine, _toneVec[0], "reduced(1x1)");
		TraceLuminanceTexel(engine, GetRT(), "adapted-before");
	}

	//Есть необходимость первого прохода без адаптации глаза.
	if (!engine.IsRestart() && !_restart)
		AdaptationLuminance(engine);

	if (sample)
		TraceLuminanceTexel(engine, GetRT(), "adapted-after");

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