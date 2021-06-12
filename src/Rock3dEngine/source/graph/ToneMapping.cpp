#include "stdafx.h"

#include "graph\\ToneMapping.h"

namespace r3d
{

namespace graph
{

Tex2DResource* ToneMapping::CreateRT()
{
	Tex2DResource* res = _MyBase::CreateRT();

	res->GetOrCreateData()->SetFormat(D3DFMT_A16B16G16R16F);

	return res;
}

void ToneMapping::Render(Engine& engine)
{
	ApplyRT(engine, RtFlags(0, 0));

	shader.Apply(engine, GetHDRTex() ? "techFinalPass" : "techFinalPassNoLum", 0);
	DrawScreenQuad(engine);
	shader.UnApply(engine);

	UnApplyRT(engine);
}

Tex2DResource* ToneMapping::GetColorTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("colorTex"));
}

void ToneMapping::SetColorTex(Tex2DResource* value)
{
	shader.SetTexture("colorTex", value);
}

Tex2DResource* ToneMapping::GetBloomTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("bloomTex"));	
}

void ToneMapping::SetBloomTex(Tex2DResource* value)
{
	shader.SetTexture("bloomTex", value);
}

Tex2DResource* ToneMapping::GetHDRTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("lumTex"));	
}

void ToneMapping::SetHDRTex(Tex2DResource* value)
{
	shader.SetTexture("lumTex", value);
}

}

}