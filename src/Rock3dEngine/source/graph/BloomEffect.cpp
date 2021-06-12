#include "stdafx.h"

#include "graph\\BloomEffect.h"

namespace r3d
{

namespace graph
{

void GetSamplesDownScale4x4(DWORD dwWidth, DWORD dwHeight, D3DXVECTOR2 avSampleOffsets[16])
{
	float tU = 1.0f / dwWidth;
    float tV = 1.0f / dwHeight;

    // Sample from 4 surrounding points. 
    int index = 0;
    for( int y = -1; y < 3; y++ )
    {
        for( int x = -1; x < 3; x++ )
        {
            avSampleOffsets[index].x = (x - 0.5f) * tU;
            avSampleOffsets[index].y = (y - 0.5f) * tV;

            index++;
        }
    }
}

inline float GaussianDistribution( float x, float y, float rho )
{
    float g = 1.0f / sqrtf( 2.0f * D3DX_PI * rho * rho );
    g *= expf( -( x * x + y * y ) / ( 2 * rho * rho ) );

    return g;
}

void CompGaussianHorizVertBlur(D3DXVECTOR2 texSize, D3DXVECTOR2 texOffsets[16], D3DXVECTOR2 colorWeights[16], float fDeviation, float fMultiplier)
{
	//Horizontal
	float tu = 1.0f / texSize.x;
	for (int i = 0; i < 8; ++i)
	{
		texOffsets[i].x = i * tu;
		colorWeights[i].x = (i > 0 ? fMultiplier : 1.0f) * GaussianDistribution((float)i, 0, fDeviation);
	}

	//Vertical
	float tv = 1.0f / texSize.y;
	for (int i = 0; i < 8; ++i)
	{
		texOffsets[i].y = i * tv;
		colorWeights[i].y = (i > 0 ? fMultiplier: 1.0f) * GaussianDistribution((float)i, 0, fDeviation);
	}

	//Copy to the left side
    for (int i = 8; i < 15; ++i)
    {
		texOffsets[i] = -texOffsets[i - 7];
		colorWeights[i] = colorWeights[i - 7];
	}
}










BloomRender::BloomRender(): _colorTex(0)
{
	_bloomTex.SetDynamic(true);
	_bloomTex.SetUsage(D3DUSAGE_RENDERTARGET);
	_bloomTex.GetOrCreateData()->SetWidth(128);
	_bloomTex.GetData()->SetHeight(128);
	_bloomTex.GetData()->SetFormat(D3DFMT_A16B16G16R16F);
}

BloomRender::~BloomRender()
{
	SetColorTex(0);

	shader.ClearTextures();
}

Tex2DResource* BloomRender::CreateRT()
{
	Tex2DResource* res = _MyBase::CreateRT();

	res->GetOrCreateData()->SetWidth(128);
	res->GetData()->SetHeight(128);
	res->GetData()->SetFormat(D3DFMT_A16B16G16R16F);

	return res;
}

void BloomRender::Render(Engine& engine)
{
	D3DXVECTOR2 samplerOffsets4x4[16];
	float  samplerWeights4x4[16];

	//Синхонизация c RT
	_bloomTex.SyncFrom(GetRT());
	_bloomTex.Init(engine);	




	//Bright pass
	GetSamplesDownScale4x4(GetRT()->GetData()->GetWidth(), GetRT()->GetData()->GetHeight(), samplerOffsets4x4);

	shader.SetValueDir("sampleOffsets4x4", samplerOffsets4x4, sizeof(samplerOffsets4x4));
	shader.SetTextureDir("colorTex", _colorTex);
	
	ApplyRT(engine, RtFlags(0, 0));
	shader.Apply(engine, GetLumTex() ? "techDown4x4BrightPass" :"techDown4x4BrightPassNoLum", 0);
	DrawScreenQuad(engine);
	shader.UnApply(engine);	
	UnApplyRT(engine);
	
		


	//Blur pass	
	D3DXVECTOR2 offsets4x4[16];
	D3DXVECTOR2 weights4x4[16];

	CompGaussianHorizVertBlur(D3DXVECTOR2((float)GetRT()->GetData()->GetWidth(), (float)GetRT()->GetData()->GetHeight()), offsets4x4, weights4x4, 3.0f, 1.25f);

	//Horizontal
	for (int i = 0; i < 16; ++i)
	{
		samplerOffsets4x4[i] = D3DXVECTOR2(offsets4x4[i].x, 0);
		samplerWeights4x4[i] = weights4x4[i].x;
	}
	shader.SetValueDir("sampleOffsets4x4", samplerOffsets4x4, sizeof(samplerOffsets4x4));
	shader.SetValueDir("sampleWeights4x4", samplerWeights4x4, sizeof(samplerWeights4x4));
	shader.SetTextureDir("colorTex", GetRT());
	
	IDirect3DSurface9* rtSurf;
	_bloomTex.GetTex()->GetSurfaceLevel(0, &rtSurf);
	engine.GetDriver().GetDevice()->SetRenderTarget(0, rtSurf);

	shader.Apply(engine, "techBloom", 0);
	DrawScreenQuad(engine);		
	shader.UnApply(engine);

	rtSurf->Release();

	//Vertical
	for (int i = 0; i < 16; ++i)
	{
		samplerOffsets4x4[i] = D3DXVECTOR2(0.0f, offsets4x4[i].y);
		samplerWeights4x4[i] = weights4x4[i].y;
	}
	shader.SetValueDir("sampleOffsets4x4", samplerOffsets4x4, sizeof(samplerOffsets4x4));
	shader.SetValueDir("sampleWeights4x4", samplerWeights4x4, sizeof(samplerWeights4x4));	
	shader.SetTextureDir("colorTex", &_bloomTex);
	
	GetRT()->GetTex()->GetSurfaceLevel(0, &rtSurf);
	engine.GetDriver().GetDevice()->SetRenderTarget(0, rtSurf);

	shader.Apply(engine, "techBloom", 0);
	DrawScreenQuad(engine);		
	shader.UnApply(engine);
	
	rtSurf->Release();	
}

Tex2DResource* BloomRender::GetColorTex()
{
	return _colorTex;
}

void BloomRender::SetColorTex(Tex2DResource* value)
{
	if (ReplaceRef(_colorTex, value))	
		_colorTex = value;
}

Tex2DResource* BloomRender::GetLumTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("lumTex"));
}

void BloomRender::SetLumTex(Tex2DResource* value)
{
	shader.SetTexture("lumTex", value);
}

}

}