#include "stdafx.h"

#include "graph\\FogPlane.h"

namespace r3d
{

namespace graph
{

FogPlane::FogPlane(): _depthTex(0), _cloudsMat(0), _color(clrWhite), _cloudIntens(0.1f), _speed(0.02f), _curTime(0)
{
	_plane.SetParent(this);
	SetSize(D3DXVECTOR2(4.0f, 4.0f));
	SetScale(100.0f);

	shader.SetTech("techVolumeFog");
}

FogPlane::~FogPlane()
{
	SetCloudsMat(0);
	SetDepthTex(0);	
}

void FogPlane::DoRender(graph::Engine& engine)
{
	const CameraCI& camera = engine.GetContext().GetCamera();

	_curTime += engine.GetDt() * _speed;
	_curTime = _curTime - static_cast<int>(_curTime);

	shader.SetValueDir("matWVP", camera.GetWVP());
	shader.SetValueDir("matWorldView", camera.GetTransform(CameraCI::ctWorldView));
	shader.SetValueDir("matInvProj", camera.GetInvProj());

	shader.SetValueDir("cloudColor", _color);
	shader.SetValueDir("cloudIntens", _cloudIntens);
	shader.SetValueDir("time", _curTime);

	shader.SetTextureDir("depthTex", _depthTex);

	//register s0
	if (_cloudsMat)
		_cloudsMat->Apply(engine);

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
	_plane.Render(engine);
	shader.UnApply(engine);

	if (_cloudsMat)
		_cloudsMat->UnApply(engine);
}

Tex2DResource* FogPlane::GetDepthTex()
{
	return _depthTex;
}

void FogPlane::SetDepthTex(Tex2DResource* value)
{
	if (ReplaceRef(_depthTex, value))
		_depthTex = value;
}

graph::LibMaterial* FogPlane::GetCloudsMat()
{
	return _cloudsMat;
}

void FogPlane::SetCloudsMat(graph::LibMaterial* value)
{
	if (ReplaceRef(_cloudsMat, value))
		_cloudsMat = value;	
}

const D3DXCOLOR& FogPlane::GetColor() const
{
	return _color;
}

void FogPlane::SetColor(const D3DXCOLOR& value)
{
	_color = value;
}

float FogPlane::GetCloudIntens() const
{
	return _cloudIntens;
}

void FogPlane::SetCloudIntens(float value)
{
	_cloudIntens = value;
}

float FogPlane::GetSpeed() const
{
	return _speed;
}

void FogPlane::SetSpeed(float value)
{
	_speed = value;
}

const D3DXVECTOR2& FogPlane::GetSize() const
{
	return _plane.GetSize();
}

void FogPlane::SetSize(const D3DXVECTOR2& value)
{
	_plane.SetSize(value);
}

}

}