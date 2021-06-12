#include "stdafx.h"

#include "graph\\DepthMap.h"

namespace r3d
{

namespace graph
{

const char* DepthMapShader::cTechniqueNames[cTechniqueTypeEnd] = {"techDepthMap", "techDepthMapAlphaTest"};




void DepthMapShader::DoBeginDraw(Engine& engine)
{
	TechniqueType tech = engine.GetContext().GetRenderState(rsAlphaTestEnable) ? ttDepthMapAlphaTest : ttDepthMap;

	ApplyTech(cTechniqueNames[tech], 0);

	//При применении прохода в нулевой регистр должна быть записана карта прозрачности
	if (tech == ttDepthMapAlphaTest)
		SetTextureDir("opacityTex", engine.GetContext().GetTexture(0));

	D3DXMATRIX matWVP;
	D3DXMatrixMultiply(&matWVP, &engine.GetContext().GetWorldMat(), &viewProjMat);
	SetValueDir("depthMatrix", matWVP);
}

bool DepthMapShader::DoEndDraw(Engine& engine, bool nextPass)
{
	bool res = _MyBase::DoEndDraw(engine, nextPass);

	UnApplyTech();

	return res;
}

DepthMapShader::TechniqueType DepthMapShader::GetTech() const
{
	return _technique;
}

void DepthMapShader::SetTech(TechniqueType value)
{
	_technique = value;

	_MyBase::SetTech(cTechniqueNames[value]);
}




Tex2DResource* DepthMapRender::CreateRT()
{
	Tex2DResource* tex = _MyBase::CreateRT();
	tex->GetOrCreateData()->SetFormat(D3DFMT_R32F);

	return tex;
}

void DepthMapRender::BeginRT(Engine& engine, const RtFlags& flags)
{
	_MyBase::BeginRT(engine, flags);

	shader.Apply(engine);

	ApplyRT(engine, flags);
}

void DepthMapRender::EndRT(Engine& engine)
{
	_MyBase::EndRT(engine);

	shader.UnApply(engine);

	UnApplyRT(engine);	
}

const D3DXMATRIX& DepthMapRender::GetViewProjMat() const
{
	return shader.viewProjMat;
}

void DepthMapRender::SetViewProjMat(const D3DXMATRIX& value)
{
	shader.viewProjMat = value;
}

}

}