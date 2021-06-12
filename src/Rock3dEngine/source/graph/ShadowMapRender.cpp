#include "stdafx.h"

#include "graph\\ShadowMapRender.h"
#include "graph\\SceneManager.h"

namespace r3d
{

namespace graph
{

const unsigned ShadowMapRender::cShadowMapSize = 2048;




void ShadowMapShader::DoBeginDraw(Engine& engine)
{
	D3DXMATRIX shadowWVP = shadowViewProj;
	D3DXMatrixMultiply(&shadowWVP, &engine.GetContext().GetWorldMat(), &shadowWVP);
	D3DXMatrixMultiply(&shadowWVP, &shadowWVP, &mTexScale);

	SetValue("matWVP", engine.GetContext().GetCamera().GetWVP());
	SetValue("matShadow", shadowWVP);
}




ShadowMapRender::ShadowMapRender(): _numSplits(0), _splitSchemeLambda(0.7f), _disableCropLight(false), _maxFar(0), _curNumSplit(0), _beginShadowCaster(false), _beginShadowMapp(false), _beginFlags(0, 0), iLight(0), _lastDepthSurface(NULL)
{
	_depthSurface = new graph::DepthStencilSurfaceResource(); 
	_depthSurface->SetWidth(cShadowMapSize);
	_depthSurface->SetHeight(cShadowMapSize);
	_depthSurface->SetFormat(D3DFMT_D24X8);	

	SetNumSplits(4);
}

ShadowMapRender::~ShadowMapRender()
{
	depthRender.SetRT(0);
	shader.ClearTextures();

	delete _depthSurface;
}

void ShadowMapRender::CalculateSplitDistances(const CameraCI& camera)
{
	// Practical split scheme:
 	// 
	// CLi = n*(f/n)^(i/numsplits)  
	// CUi = n + (f-n)*(i/numsplits)  
	// Ci = CLi*(lambda) + CUi*(1-lambda)  
	// 
	// lambda scales between logarithmic and uniform 
	//
	float fCameraNear = camera.GetDesc().nearDist;
	float fCameraFar = camera.GetDesc().farDist;
	if (_maxFar != 0.0f)
		fCameraFar = std::min(fCameraFar, _maxFar);

	for (unsigned i = 0; i < _numSplits; ++i)
	{
		float fIDM = i / static_cast<float>(_numSplits);
		float fLog = fCameraNear * powf((fCameraFar / fCameraNear), fIDM);
		float fUniform = fCameraNear + (fCameraFar - fCameraNear) * fIDM;    
		_splitDistances[i] = fLog * _splitSchemeLambda + fUniform * (1 - _splitSchemeLambda);
	}
	// make sure border values are right
	_splitDistances[0] = fCameraNear;
	_splitDistances[_numSplits] = fCameraFar;
}

void ShadowMapRender::ComputeCropMatrix(unsigned numSplit, const LightCI& light, const Frustum::Corners& pCorners)
{
	float fMaxX = -FLT_MAX;
	float fMaxY = -FLT_MAX;
	float fMaxZ = -FLT_MAX;
	float fMinX = FLT_MAX;
	float fMinY = FLT_MAX;
	float fMinZ = FLT_MAX;

	const D3DXMATRIX& mLightViewProj = light.GetCamera().GetViewProj();
	float fLightNear = light.GetCamera().GetDesc().nearDist;	

	// for each corner point
	for (int i = 0; i < 8; ++i)
	{
		// transform point
		D3DXVECTOR4 vTransformed;
		D3DXVec3Transform(&vTransformed, &pCorners[i], &mLightViewProj);

		// project x and y
		vTransformed.x /= vTransformed.w;
		vTransformed.y /= vTransformed.w;

		// find min and max values
		if (vTransformed.x > fMaxX) fMaxX = vTransformed.x;
		if (vTransformed.y > fMaxY) fMaxY = vTransformed.y;		
		if (vTransformed.y < fMinY) fMinY = vTransformed.y;
		if (vTransformed.x < fMinX) fMinX = vTransformed.x;

		// find largest z distance
		if (vTransformed.z > fMaxZ) fMaxZ = vTransformed.z;
		if (vTransformed.z < fMinZ) fMinZ = vTransformed.z;
	}

	//Обрезать проекционные границы до доп. значений, приводит к обрезанию теней
	//fMaxX = lsl::ClampValue(fMaxX, -1.0f, 1.0f);
	//fMaxY = lsl::ClampValue(fMaxY, -1.0f, 1.0f);
	//fMinX = lsl::ClampValue(fMinX, -1.0f, 1.0f);
	//fMinY = lsl::ClampValue(fMinY, -1.0f, 1.0f);
	//Если сплит камеры частично или полностью находится за пределами пирамиды ист. света, подгонять проекционную матрицу в данных направлениях нельзя(тени будут обрезаться)
	if (fMaxX < -1.0f || fMaxX > 1.0f ||
		fMinX < -1.0f || fMinX > 1.0f)
	{
		fMaxX = 1.0f;
		fMinX = -1.0f;
	}
	if (fMaxY < -1.0f || fMaxY > 1.0f ||
		fMinY < -1.0f || fMinY > 1.0f)
	{
		fMaxY = 1.0f;
		fMinY = -1.0f;
	}

	// Use default near-plane value 
	fMinZ = 0.0f;
	// Adjust the far plane of the light to be at the farthest
	// point of the frustum split. Some bias may be necessary.
	fMaxZ += 1.5f;

	_lightDist[numSplit] = fMaxZ;

	// Next we build a special matrix for cropping the lights view
	// to only contain points of the current frustum split
	//
	float fScaleX = 2.0f / (fMaxX - fMinX);
	float fScaleY = 2.0f / (fMaxY - fMinY);
	float fScaleZ = 1.0f / (fMaxZ - fMinZ);

	float fOffsetX = -0.5f * (fMaxX + fMinX) * fScaleX;
	float fOffsetY = -0.5f * (fMaxY + fMinY) * fScaleY;
	float fOffsetZ = -fMinZ * fScaleZ;

	D3DXMATRIX mCropView(fScaleX,     0.0f,  0.0f,   0.0f,
                            0.0f,  fScaleY,  0.0f,   0.0f,
                            0.0f,     0.0f,  fScaleZ,   0.0f,
                        fOffsetX, fOffsetY,  fOffsetZ,   1.0f);

	// multiply the projection matrix with it
	//Итоговое значение глубины в z буффере будет линейным, в диапазоне от 0 до 1 (near; far), которое можно вычислить как:
	//depth = Zf/maxZ * (Z - Zn)/(Zf - Zn);
	//Приблизительно depth = Z / maxZ.
	_splitLightProjMat[numSplit] = light.GetCamera().GetProj() * mCropView;
}

void ShadowMapRender::CalcSplitScheme(const CameraCI& camera, const LightCI& light)
{
	CalculateSplitDistances(camera);

	for (unsigned i = 0; i < _numSplits; ++i)
	{
		CameraDesc desc = camera.GetDesc();
		desc.nearDist = _splitDistances[i];
		desc.farDist = _splitDistances[i+1];
		CameraCI tmpCamera;
		tmpCamera.SetDesc(desc);

		if (_disableCropLight)
		{
			D3DXMATRIX mCropView(
				1, 0.0f,  0.0f,   0.0f,
				0.0f, 1, 0.0f, 0.0f,
				0.0f, 0.0f, 1.0f/(desc.farDist - desc.nearDist), 0.0f,
				0.0f, 0.0f, 0.0f, 1.0f
			);
			_splitLightProjMat[i] = light.GetCamera().GetProj() * mCropView;
		}
		else
		{
			Frustum::Corners corners;
			Frustum::CalculateCorners(corners, tmpCamera.GetInvViewProj());
			ComputeCropMatrix(i, light, corners);
		}
	}
}

Tex2DResource* ShadowMapRender::CreateRT()
{
	Tex2DResource* res = _MyBase::CreateRT();

	res->GetOrCreateData()->SetFormat(D3DFMT_X8R8G8B8);

	return res;
}

void ShadowMapRender::BeginShadowCaster(Engine& engine)
{
	LSL_ASSERT(!_beginShadowCaster);

	_beginShadowCaster = true;

	if (_curNumSplit == 0)
	{
		CameraDesc lightCamDesc = engine.GetContext().GetLight(iLight).GetCamera().GetDesc();
		_myCamera.SetDesc(lightCamDesc);

		engine.GetDriver().GetDevice()->GetDepthStencilSurface(&_lastDepthSurface);
		engine.GetDriver().GetDevice()->SetDepthStencilSurface(_depthSurface->GetSurface());
	}

	D3DXMATRIX viewProj = _myCamera.GetView() * _splitLightProjMat[_curNumSplit];
	depthRender.SetViewProjMat(viewProj);

	engine.GetContext().ApplyCamera(&_myCamera);

	depthRender.SetRT(_shadowVec[_curNumSplit]);
	depthRender.BeginRT(engine, RtFlags(0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, 0xFFFFFFFF));
}

bool ShadowMapRender::EndShadowCaster(Engine& engine, bool nextPass)
{
	LSL_ASSERT(_beginShadowCaster);

	_beginShadowCaster = false;
	
	depthRender.EndRT(engine);
	engine.GetContext().UnApplyCamera(&_myCamera);

	if (nextPass && ++_curNumSplit < _numSplits)
	{
		BeginShadowCaster(engine);
		return false;
	}
	else
	{
		_curNumSplit = 0;

		engine.GetDriver().GetDevice()->SetDepthStencilSurface(_lastDepthSurface);
		lsl::SafeRelease(_lastDepthSurface);

		return true;
	}		
}

void ShadowMapRender::BeginShadowMapp(Engine& engine)
{
	LSL_ASSERT(!_beginShadowMapp);

	_beginShadowMapp = true;

	if (_curNumSplit == 0)
	{
		float mapSz = static_cast<float>(cShadowMapSize);
		float fTexOffset = 0.5f + 0.5f / mapSz;
		shader.mTexScale = D3DXMATRIX(0.5f,       0.0f,       0.0f,   0.0f,
			                          0.0f,       -0.5f,      0.0f,   0.0f,
							          0.0f,       0.0f,       1.0f,   0.0f,
							          fTexOffset, fTexOffset, 0.0f,   1.0f);

		shader.SetValue("sizeShadow", D3DXVECTOR2(mapSz, 1.0f / mapSz));

		ApplyRT(engine, _beginFlags);
		engine.GetDriver().GetDevice()->GetViewport(&_oldViewPort);
		engine.GetContext().SetIgnoreMaterial(true);

		shader.Apply(engine, "techLightMap", 0);
	}

	shader.SetTexture("shadowTex", _shadowVec[_curNumSplit]);

	// Since the near and far planes are different for each
	// rendered split, we need to change the depth value range
	// to avoid rendering over previous splits				
	// as long as ranges are in order and don't overlap it should be all good...
	D3DVIEWPORT9 cameraViewport = _oldViewPort;
	cameraViewport.MinZ = _curNumSplit / static_cast<float>(_numSplits);
	cameraViewport.MaxZ = (_curNumSplit + 1) / static_cast<float>(_numSplits);	
	engine.GetDriver().GetDevice()->SetViewport(&cameraViewport);

	CameraDesc desc = engine.GetContext().GetCamera().GetDesc();
	desc.nearDist = _splitDistances[_curNumSplit];
	desc.farDist = _splitDistances[_curNumSplit + 1];
	_myCamera.SetDesc(desc);

	D3DXMatrixMultiply(&shader.shadowViewProj, &engine.GetContext().GetLight(iLight).GetCamera().GetView(), &_splitLightProjMat[_curNumSplit]);

	engine.GetContext().ApplyCamera(&_myCamera);
}

bool ShadowMapRender::EndShadowMapp(Engine& engine, bool nextPass)
{
	LSL_ASSERT(_beginShadowMapp);

	_beginShadowMapp = false;

	engine.GetContext().UnApplyCamera(&_myCamera);
	
	if (nextPass && ++_curNumSplit < _numSplits)
	{
		BeginShadowMapp(engine);
		return false;
	}
	else
	{
		shader.UnApply(engine);

		engine.GetContext().SetIgnoreMaterial(false);
		UnApplyRT(engine);
		_curNumSplit = 0;
		return true;
	}
}

void ShadowMapRender::BeginRT(Engine& engine, const RtFlags& flags)
{
	_beginFlags = flags;

	_beginFlags.clearFlags = D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER;
	_beginFlags.color = 0xFFFFFFFF;

	_depthSurface->Init(engine);

	for (Textures::iterator iter = _shadowMaps.begin(); iter != _shadowMaps.end(); ++iter)
		(*iter)->Init(engine);

	CalcSplitScheme(engine.GetContext().GetCamera(), engine.GetContext().GetLight(iLight));
}

void ShadowMapRender::EndRT(Engine& engine)
{
	//Nothing
}

void ShadowMapRender::RenderDebug(Engine& engine)
{
	const unsigned numSplit = 0;

	CameraDesc lightCamDesc = engine.GetContext().GetLight(iLight).GetCamera().GetDesc();
	_myCamera.SetDesc(lightCamDesc);

	_myCamera.SetProjMat(_splitLightProjMat[numSplit]);
	CameraDesc desc = _myCamera.GetDesc();
	desc.farDist = _lightDist[numSplit];
	_myCamera.SetDesc(desc);

	Camera::RenderFrustum(engine, _myCamera.GetInvViewProj(), clrGreen);
	
}

unsigned ShadowMapRender::GetNumSplits() const
{
	return _numSplits;
}

void ShadowMapRender::SetNumSplits(unsigned value)
{
	if (_numSplits != value)
	{
		value = std::max(value, 1U);

		_numSplits = value;
		_splitDistances.resize(_numSplits + 1);
		_lightDist.resize(_numSplits);
		_splitLightProjMat.resize(_numSplits);

		//удаление всех текстур		
		_shadowMaps.Clear();
		//
		_shadowVec.clear();
		//создание		
		for (unsigned i = 0; i < _numSplits; ++i)
		{
			Tex2DResource& tex = _shadowMaps.Add();

			tex.SetDynamic(true);
			tex.SetUsage(D3DUSAGE_RENDERTARGET);
			tex.GetOrCreateData()->SetWidth(cShadowMapSize);
			tex.GetData()->SetHeight(cShadowMapSize);
			tex.GetData()->SetFormat(D3DFMT_R32F);

			_shadowVec.push_back(&tex);
		}
	}
}

float ShadowMapRender::GetSplitSchemeLambda() const
{
	return _splitSchemeLambda;	
}

void ShadowMapRender::SetSplitSchemeLambda(float value)
{
	_splitSchemeLambda = lsl::ClampValue(value, 0.0f, 1.0f);
}

bool ShadowMapRender::GetDisableCropLight() const
{
	return _disableCropLight;
}

void ShadowMapRender::SetDisableCropLight(bool value)
{
	_disableCropLight = value;
}

float ShadowMapRender::GetMaxFar() const
{
	return _maxFar;
}

void ShadowMapRender::SetMaxFar(float value)
{
	_maxFar = value;
}




CombineLightMap::~CombineLightMap()
{
	ClearLightMapList();
}

void CombineLightMap::InsertLightMap(Tex2DResource* value)
{
	_lightMapList.push_back(value);

	value->AddRef();
}

void CombineLightMap::RemoveLightMap(Tex2DResource* value)
{
	_lightMapList.remove(value);

	value->Release();
}

void CombineLightMap::ClearLightMapList()
{
	for (LightMapList::iterator iter = _lightMapList.begin(); iter != _lightMapList.end(); ++iter)
		(*iter)->Release();

	_lightMapList.clear();
}

void CombineLightMap::Render(Engine& engine)
{
	shader.SetValue("numLights", static_cast<float>(_lightMapList.size()));
	shader.Apply(engine, "techCombLightMap", 0);

	ApplyRT(engine, RtFlags(0, 0));

	for (LightMapList::iterator iter = _lightMapList.begin(); iter != _lightMapList.end(); ++iter)
	{
		//Со следующего прохода включается блендинг чтобы суммировать результаты проходов
		if (iter != _lightMapList.begin())
		{
			engine.GetContext().SetRenderState(rsAlphaBlendEnable, true);
			engine.GetContext().SetRenderState(rsDestBlend, D3DBLEND_ONE);
			engine.GetContext().SetRenderState(rsSrcBlend, D3DBLEND_ONE);
		}

		shader.SetTexture("lightMapTex", *iter);
		DrawScreenQuad(engine);
	}

	UnApplyRT(engine);

	engine.GetContext().RestoreRenderState(rsAlphaBlendEnable);
	engine.GetContext().RestoreRenderState(rsDestBlend);
	engine.GetContext().RestoreRenderState(rsSrcBlend);

	shader.UnApply(engine);
}

const CombineLightMap::LightMapList& CombineLightMap::GetLightMapList() const
{
	return _lightMapList;
}




void MappingLightMap::Render(Engine& engine)
{
	shader.Apply(engine, "techMappingLightMap", 0);

	ApplyRT(engine, RtFlags(0, 0));
	DrawScreenQuad(engine);
	UnApplyRT(engine);	

	shader.UnApply(engine);
}

Tex2DResource* MappingLightMap::GetColorTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("colorTex"));
}

void MappingLightMap::SetColorTex(Tex2DResource* value)
{
	shader.SetTexture("colorTex", value);
}

Tex2DResource* MappingLightMap::GetLightMapTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("lightMapTex"));
}

void MappingLightMap::SetLightMapTex(Tex2DResource* value)
{
	shader.SetTexture("lightMapTex", value);
}

}

}