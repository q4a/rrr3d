#include "stdafx.h"

#include "graph\\WaterPlane.h"

namespace r3d
{

namespace graph
{

ReflRender::ReflRender()
{
	SetReflPlane(D3DXPLANE(0.0f, 0.0f, 1.0f, 0.0f));
}

void ReflRender::BeginRT(Engine& engine, const RtFlags& flags)
{
	const DWORD cClipPlanes[6] = {D3DCLIPPLANE0, D3DCLIPPLANE1, D3DCLIPPLANE2, D3DCLIPPLANE3, D3DCLIPPLANE4, D3DCLIPPLANE5};

	_MyBase::BeginRT(engine, flags);

	CameraDesc desc = engine.GetContext().GetCamera().GetDesc();
	D3DXVec3TransformCoord(&desc.pos, &desc.pos, &_reflMat);
	D3DXVec3TransformNormal(&desc.dir, &desc.dir, &_reflMat);
	D3DXVec3TransformNormal(&desc.up, &desc.up, &_reflMat);
	desc.up = -desc.up;
	_reflCamera.SetDesc(desc);
	//reflCamera.AdjustFarPlane(sceneBox, maxFarDist);

	engine.GetContext().ApplyCamera(&_reflCamera);

	DWORD enableClipPlanes = cClipPlanes[0];
	engine.GetDriver().GetDevice()->SetClipPlane(0, _reflPlane);

	LSL_ASSERT(_clipPlanes.size() <= 5);

	for (unsigned i = 0; i < _clipPlanes.size(); ++i)
	{
		enableClipPlanes |= cClipPlanes[i + 1];
		engine.GetDriver().GetDevice()->SetClipPlane(i + 1, _clipPlanes[i]);
	}

	engine.GetContext().SetRenderState(rsClipPlaneEnable, enableClipPlanes);

	ApplyRT(engine, flags);
}

void ReflRender::EndRT(Engine& engine)
{
	_MyBase::EndRT(engine);

	UnApplyRT(engine);

	engine.GetContext().RestoreRenderState(rsClipPlaneEnable);
	engine.GetContext().UnApplyCamera(&_reflCamera);
}

const D3DXPLANE& ReflRender::GetReflPlane() const
{
	return _reflPlane;
}

void ReflRender::SetReflPlane(const D3DXPLANE& value)
{
	_reflPlane = value;
	D3DXMatrixReflect(&_reflMat, &_reflPlane);
}

const ReflRender::ClipPlanes& ReflRender::GetClipPlanes() const
{
	return _clipPlanes;
}

void ReflRender::SetClipPlanes(const ClipPlanes& value)
{
	_clipPlanes = value;
}




WaterPlane::WaterPlane(): _viewPos(NullVector), _cloudIntens(0.1f)
{
	shader.SetValue("normalScale", 1.0f);
	shader.SetTech("techWater");
}

void WaterPlane::DoRender(graph::Engine& engine)
{
	shader.SetValueDir("matWorld", engine.GetContext().GetWorldMat());
	shader.SetValueDir("matWorldView", engine.GetContext().GetCamera().GetTransform(CameraCI::ctWorldView));
	shader.SetValueDir("matWVP", engine.GetContext().GetCamera().GetWVP());
	shader.SetValueDir("matInvProj", engine.GetContext().GetCamera().GetInvProj());
	shader.SetValueDir("lightPos", !engine.GetContext().GetLights().empty() ? engine.GetContext().GetLight(0).GetDesc().pos : NullVector);
	shader.SetValueDir("viewPos", _viewPos);

	static float curTime = 0;
	curTime += engine.GetDt() * 0.15f;
	curTime = curTime - static_cast<int>(curTime);

	shader.SetValueDir("time", curTime);
	shader.SetValueDir("cloudIntens", _cloudIntens);

	D3DXVECTOR3 fogParamsVec = D3DXVECTOR3(0, 1, (float)engine.GetContext().GetRenderState(rsFogEnable));
	if (fogParamsVec.z != 0)
	{
		DWORD dwVal = engine.GetContext().GetRenderState(rsFogStart);
		fogParamsVec.x = *(float*)(&dwVal);

		dwVal = engine.GetContext().GetRenderState(rsFogEnd);
		fogParamsVec.y = *(float*)(&dwVal);

		D3DXCOLOR fogColorVec = D3DXCOLOR(engine.GetContext().GetRenderState(rsFogColor));
		shader.SetValueDir("fogColor", fogColorVec);
	}
	shader.SetValueDir("fogParams", fogParamsVec);

	shader.Apply(engine);
	_MyBase::DoRender(engine);
	shader.UnApply(engine);
}

Tex2DResource* WaterPlane::GetDepthTex()
{
	return static_cast<Tex2DResource*>(shader.GetTexture("depthTex"));
}

void WaterPlane::SetDepthTex(Tex2DResource* value)
{
	shader.SetTexture("depthTex", value);
}

graph::Tex2DResource* WaterPlane::GetNormTex()
{
	return static_cast<Tex2DResource*>(shader.GetTexture("normalTex"));
}

void WaterPlane::SetNormTex(graph::Tex2DResource* value)
{
	shader.SetTexture("normalTex", value);
}

graph::Tex2DResource* WaterPlane::GetReflTex()
{
	return static_cast<Tex2DResource*>(shader.GetTexture("reflTex"));
}

void WaterPlane::SetReflTex(graph::Tex2DResource* value)
{
	shader.SetTexture("reflTex", value);
}

D3DXCOLOR WaterPlane::GetColor()
{
	D3DXCOLOR res;
	if (shader.GetValue("waterColor", res))
		return res;
	else
		return clrBlack;
}

void WaterPlane::SetColor(const D3DXCOLOR& value)
{
	shader.SetValue("waterColor", D3DXVECTOR4(value));
}

const D3DXVECTOR3& WaterPlane::GetViewPos() const
{
	return _viewPos;
}

void WaterPlane::SetViewPos(const D3DXVECTOR3& value)
{
	_viewPos = value;
}

float WaterPlane::GetCloudIntens() const
{
	return _cloudIntens;
}

void WaterPlane::SetCloudIntens(float value)
{
	_cloudIntens = value;
}

}

}