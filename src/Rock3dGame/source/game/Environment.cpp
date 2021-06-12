#include "stdafx.h"
#include "game\World.h"

#include "game\Environment.h"

namespace r3d
{

namespace game
{

const std::string Environment::cWheaterStr[cWheaterEnd] = {"ewFair", "ewNight", "ewClody", "ewRainy", "ewSahara", "ewHell", "ewSnow"};

const lsl::string Environment::cSyncFrameRateStr[cSyncFrameRateEnd] = {"sfrNone", "sfrFixed"};

GraphManager::GraphQuality Environment::cGraphQuality[Environment::cQualityEnd] = {GraphManager::gqLow, GraphManager::gqMiddle, GraphManager::gqHigh};

int Environment::cFilteringLevel[Environment::cFilteringEnd] = {0, 2, 4, 8, 16};

int Environment::cMultisamplingLevel[Environment::cMultisamplingEnd] = {0, 2, 4, 8, 16};

std::pair<GraphManager::GraphOption, Environment::Quality> Environment::cShadowGraphMap[] = {
	std::make_pair(GraphManager::goPixelLighting, Environment::eqMiddle),
	std::make_pair(GraphManager::goShadow, Environment::eqMiddle)
};

std::pair<GraphManager::GraphOption, Environment::Quality> Environment::cLightGraphMap[] = {
	std::make_pair(GraphManager::goPlanarRefl, Environment::eqHigh),
	std::make_pair(GraphManager::goBumpMap, Environment::eqHigh),
	std::make_pair(GraphManager::goPixelLighting, Environment::eqMiddle),
	std::make_pair(GraphManager::goRefl, Environment::eqMiddle),
	std::make_pair(GraphManager::goTrueRefl, Environment::eqHigh)
	//std::make_pair(GraphManager::goPixelLighting, Environment::eqMiddle)
};

std::pair<GraphManager::GraphOption, Environment::Quality> Environment::cPostEffGraphMap[] = {
	std::make_pair(GraphManager::goHDR, Environment::eqHigh),
	std::make_pair(GraphManager::goBloom, Environment::eqMiddle),
	std::make_pair(GraphManager::goSunShaft, Environment::eqHigh),
	std::make_pair(GraphManager::goRefr, Environment::eqMiddle)
};

std::pair<GraphManager::GraphOption, Environment::Quality> Environment::cEnvGraphMap[] = {
	std::make_pair(GraphManager::goSkyBox, Environment::eqMiddle),
	std::make_pair(GraphManager::goWater, Environment::eqLow),
	std::make_pair(GraphManager::goGrassField, Environment::eqLow),
	std::make_pair(GraphManager::goFog, Environment::eqLow),
	std::make_pair(GraphManager::goPlaneFog, Environment::eqLow),
	std::make_pair(GraphManager::goMagma, Environment::eqLow)
};




Environment::Environment(World* world): _world(world), _wheater(ewClody), _worldType(wtWorld1), _syncFrameRate(sfrFixed), _sun(0), _shadowQuality(eqLow), _lightQuality(eqLow), _postEffQuality(eqLow), _envQuality(eqLow), _filtering(efLinear), _multisampling(emNone), _editMode(false), _startScene(false), _enableRain(false), _rain(0)
{
	_sunPos = D3DXVECTOR3(150, 150, 150.0f);
	_sunRot = NullQuaternion;

	ZeroMemory(_lamp, sizeof(_lamp));
	ZeroMemory(_lampPos, sizeof(_lampPos));
	for (int i = 0; i < 4; ++i)
		_lampRot[i] = NullQuaternion;
	for (int i = 0; i < 4; ++i)
		_lampColor[i] = clrWhite;
	for (int i = 0; i < 4; ++i)
		_lampSwitchOn[i] = true;

	D3DXQUATERNION rot1;
	D3DXQuaternionRotationAxis(&rot1, &YVector, D3DX_PI/4.0f);
	D3DXQUATERNION rot2;
	D3DXQuaternionRotationAxis(&rot2, &ZVector, -D3DX_PI/4.0f);	
	_sunRot = rot1 * rot2;

	//текстура по умолчанию. Обязательно должна быть!
	GetGraph()->SetSkyTex("Data\\World1\\Texture\\skyTex1.dds");	
}

Environment::~Environment()
{
	EnableSun(false);
	EnableLamps(false);
}

void Environment::CreateRain()
{
	if (_rain)
		return;

	_isoRain = GetCamera() ? GetCamera()->GetStyle() == CameraManager::csIsometric : false;

	//ignore in iso mode
	if (_isoRain)
		return;

	_rain = &_world->GetMap()->AddMapObj(_world->GetDB()->GetRecord(MapObjLib::ctEffects, _isoRain ? "rainIso" : "rain"));
	_rain->AddRef();
}

void Environment::FreeRain()
{
	if (_rain)
	{
		_rain->Release();
		_world->GetMap()->DelMapObj(_rain);
		_rain = 0;
	}
}

void Environment::ApplyRain()
{
	FreeRain();
	if (_startScene && _enableRain)
		CreateRain();
}

void Environment::EnableSun(bool enable, bool enableShadow)
{
	if (enable && !_sun)
	{
		GraphManager::LightDesc desc;
		desc.shadow = enableShadow;
		desc.shadowNumSplit = 2;
		desc.shadowDisableCropLight = false;
		//минимальный nearDist, от него зависит точность линейной глубины в depthMap, чем ниже тем точность меньше. Очень низкие значения могут привести к артефактам в тенях, в виде дрожания
		desc.nearDist = 10;
		desc.farDist = 400;
		_sun = _world->GetGraph()->AddLight(desc);
		_sun->AddRef();
		//солнце создает амбиент и освешение, сумма равна 1,2 чтобы подсветить сцену при программируемом конвеере
		_sun->GetSource()->SetType(D3DLIGHT_DIRECTIONAL);
		_sun->GetSource()->SetAmbient(clrGray60);
		_sun->GetSource()->SetDiffuse(clrGray60);
		_sun->GetSource()->SetFalloff(0);		
		_sun->GetSource()->SetPos(_sunPos);
		_sun->GetSource()->SetRot(_sunRot);

		_world->GetCamera()->SetLight(_sun);
	}
	else if (!enable && _sun)
	{
		if (_world->GetCamera()->GetLight() == _sun)
			_world->GetCamera()->SetLight(0);

		_sun->Release();
		_world->GetGraph()->DelLight(_sun);
		_sun = 0;
	}
	else if (enable && _sun && _sun->GetDesc().shadow != enableShadow)
	{
		GraphManager::LightDesc desc = _sun->GetDesc();
		desc.shadow = enableShadow;

		_world->GetGraph()->SetLight(_sun, desc);
	}
}

void Environment::EnableLamp(bool enable, int index, float farDist)
{
	if (enable && !_lamp[index])
	{
		GraphManager::LightDesc desc;
		desc.shadow = true;
		desc.shadowNumSplit = 1;
		desc.shadowDisableCropLight = true;
		//минимальный nearDist, от него зависит точность линейной глубины в depthMap, чем ниже тем точность меньше. Очень низкие значения могут привести к артефактам в тенях, в виде дрожания
		desc.nearDist = 1.0f;
		desc.farDist = farDist;
		_lamp[index] = _world->GetGraph()->AddLight(desc);
		_lamp[index]->AddRef();
		//солнце создает амбиент и освешение, сумма равна 1,2 чтобы подсветить сцену при программируемом конвеере
		_lamp[index]->GetSource()->SetType(D3DLIGHT_SPOT);
		_lamp[index]->GetSource()->SetAmbient(clrBlack);
		_lamp[index]->GetSource()->SetDiffuse(_lampColor[index]);
		_lamp[index]->SetEnable(_lampSwitchOn[index]);
		//_lamp->GetSource()->SetFalloff(0);		
		_lamp[index]->GetSource()->SetPos(_lampPos[index]);
		_lamp[index]->GetSource()->SetRot(_lampRot[index]);

		_world->GetCamera()->SetLight(_lamp[index]);
	}
	else if (!enable && _lamp[index])
	{
		if (_world->GetCamera()->GetLight() == _lamp[index])
			_world->GetCamera()->SetLight(0);

		_lamp[index]->Release();
		_world->GetGraph()->DelLight(_lamp[index]);
		_lamp[index] = 0;
	}
	else if (_lamp[index])
	{
		GraphManager::LightDesc desc = _lamp[index]->GetDesc();
		desc.farDist = farDist;
		_world->GetGraph()->SetLight(_lamp[index], desc);
	}
}

void Environment::EnableLamps(bool enable)
{
	for (int i = 0; i < 4; ++i)
		EnableLamp(enable, i);
}

void Environment::EnableWater(bool enable)
{
	SetGraphOption(GraphManager::goWater, enable);	
}

void Environment::EnablePlanarRefl(bool enable)
{
	SetGraphOption(GraphManager::goPlanarRefl, enable);	
}

void Environment::EnableGrass(bool enable)
{
	SetGraphOption(GraphManager::goGrassField, enable);
}

void Environment::EnableGroundFog(bool enable)
{
	SetGraphOption(GraphManager::goPlaneFog, enable);
	if (enable)
	{
		GetGraph()->SetCloudIntensivity(0.1f);
		GetGraph()->SetCloudHeight(3.0f);
	}
}

void Environment::EnableMagma(bool enable)
{
	SetGraphOption(GraphManager::goMagma, enable);
	if (enable)
	{
		GetGraph()->SetCloudIntensivity(1.0f);
		GetGraph()->SetCloudHeight(0.5f);
	}
}

void Environment::EnableRain(bool enable)
{
	_enableRain = enable;
	ApplyRain();
}

bool Environment::CheckGraphMap(GraphManager::GraphOption option, Quality quality, std::pair<GraphManager::GraphOption, Quality> graphMap[], int count, Quality& resQuality)
{
	resQuality = eqHigh;

	for (int i = 0; i < count; ++i)
	{
		if (graphMap[i].first == option)
		{
			resQuality = quality;
			return quality >= graphMap[i].second;
		}
	}

	return false;
}

bool Environment::IsGraphMapSupported(Quality value, std::pair<GraphManager::GraphOption, Quality> graphMap[], int count)
{
	for (int i = 0; i < count; ++i)
		if (value >= graphMap[i].second && GetGraph()->IsGraphOptionSupported(graphMap[i].first, cGraphQuality[value]) == false)
			return false;

	return true;
}

void Environment::SetGraphOption(GraphManager::GraphOption option, bool value)
{
	Environment::Quality quality = Environment::eqHigh;

	value = value && (CheckGraphMap(option, _shadowQuality, cShadowGraphMap, ARRAY_LENGTH(cShadowGraphMap), quality) ||
		CheckGraphMap(option, _lightQuality, cLightGraphMap, ARRAY_LENGTH(cLightGraphMap), quality) ||
		CheckGraphMap(option, _postEffQuality, cPostEffGraphMap, ARRAY_LENGTH(cPostEffGraphMap), quality) || 
		CheckGraphMap(option, _envQuality, cEnvGraphMap, ARRAY_LENGTH(cEnvGraphMap), quality));

	GetGraph()->SetGraphOption(option, value, cGraphQuality[quality]);
}

void Environment::ApplyCloudColor()
{
	D3DXCOLOR cloudColor = GetGraph()->GetFogColor();
	if (_worldType == wtWorld3)
		cloudColor = D3DXCOLOR(87.0f/255.0f, 81.0f/255.0f, 115.0f/255.0f, 1.0f);		
	else if (_worldType == wtWorld4)
		cloudColor = clrWhite;

	GetGraph()->SetCloudColor(cloudColor);
}

void Environment::ApplyWheater()
{
	float cameraFar = _world->GetCamera()->GetStyle() == CameraManager::csIsometric ? GetOrthoCameraFar() : GetPerspectiveCameraFar();

	switch (_wheater)
	{
	case ewFair:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(148.0f/255.0f, 193.0f/255.0f, 235.0f/255.0f, 1.0f);

		GetGraph()->SetSceneAmbient(clrBlack);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(0.5f);
		GetGraph()->SetSkyTex("Data\\World1\\Texture\\skyTex1.dds");
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      true);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           true);
		
		EnableSun(true);
		EnableRain(false);		
		break;
	}
		
	case ewNight:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(15.0f, 25.0f, 31.0f, 255.0f)/255.0f;

		GetGraph()->SetSceneAmbient(D3DXCOLOR(0xFF8A90AE));
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(1.0f);
		GetGraph()->SetSkyTex("Data\\Misc\\nightSky.dds");
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      false);
		SetGraphOption(GraphManager::goBloom,         false);
		SetGraphOption(GraphManager::goHDR,           false);
		SetGraphOption(GraphManager::goFog,           true);
		
		EnableSun(false);
		EnableRain(false);
		break;
	}
		
	case ewClody:
	{
		D3DXCOLOR fogColor = 0x00C0BDB8;

		GetGraph()->SetSkyTex("Data\\World2\\Texture\\skyTex1.dds");
		GetGraph()->SetSceneAmbient(clrBlack);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(1.0f);
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      true);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           true);

		EnableSun(true);
		EnableRain(false);
		break;
	}
		
	case ewRainy:
	{
		D3DXCOLOR fogColor = 0x00C0BDB8;

		GetGraph()->SetSceneAmbient(clrBlack);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(1.0f);
		GetGraph()->SetSkyTex("Data\\World2\\Texture\\skyTex1.dds");
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      true);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           true);

		EnableSun(true);
		EnableRain(true);
		break;
	}

	case ewSahara:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(87.0f/255.0f, 81.0f/255.0f, 115.0f/255.0f, 1.0f);

		GetGraph()->SetSceneAmbient(clrBlack);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(0.5f);
		GetGraph()->SetSkyTex("Data\\World3\\Texture\\skyTex1.dds");
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      true);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           true);

		EnableSun(true);
		EnableRain(false);	
		break;
	}

	case ewHell:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(82.0f/255.0f, 12.0f/255.0f, 8.0f/255.0f, 1.0f);

		GetGraph()->SetSceneAmbient(clrBlack);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(0.5f);
		GetGraph()->SetSkyTex("Data\\World4\\Texture\\skyTex1.dds");		
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      true);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           true);

		EnableSun(true);
		EnableRain(false);
		break;
	}

	case ewSnow:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(0xFF9CA6B5);

		GetGraph()->SetSceneAmbient(clrBlack);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(0.5f);
		GetGraph()->SetSkyTex("Data\\World5\\Texture\\sky_text.dds");		
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        true);
		SetGraphOption(GraphManager::goSunShaft,      true);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           true);

		EnableSun(true, _shadowQuality == eqHigh);
		EnableRain(false);
		break;
	}

	case ewGarage:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(148.0f/255.0f, 193.0f/255.0f, 235.0f/255.0f, 1.0f);

		GetGraph()->SetSceneAmbient(clrGray60);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(1.0f);
		GetGraph()->SetSkyTex("Data\\World1\\Texture\\skyTex1.dds");		
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        false);
		SetGraphOption(GraphManager::goSunShaft,      false);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           false);
		
		EnableSun(false);
		EnableRain(false);
		break;
	}

	case ewAngar:
	{
		D3DXCOLOR fogColor = D3DXCOLOR(148.0f/255.0f, 193.0f/255.0f, 235.0f/255.0f, 1.0f);

		GetGraph()->SetSceneAmbient(clrGray60);
		GetGraph()->SetFogColor(fogColor);
		GetGraph()->SetFogIntensivity(1.0f);
		GetGraph()->SetSkyTex("Data\\World1\\Texture\\skyTex1.dds");		
		GetCamera()->SetFar(cameraFar);

		SetGraphOption(GraphManager::goSkyBox,        false);
		SetGraphOption(GraphManager::goSunShaft,      false);
		SetGraphOption(GraphManager::goBloom,         true);
		SetGraphOption(GraphManager::goHDR,           true);
		SetGraphOption(GraphManager::goFog,           false);
		
		EnableSun(false);
		EnableRain(false);
		break;
	}

	}

	ApplyCloudColor();
}

void Environment::ApplyWorldType()
{
	switch (_worldType)
	{
	case wtWorld1:
	{
		EnableLamps(false);
		EnableWater(false);
		EnableGrass(true);
		EnableGroundFog(false);
		EnableMagma(false);
		EnablePlanarRefl(false);
		
		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 1.1f;
		hdrParams.brightThreshold = 1.5f;
		hdrParams.gaussianScalar = 30.0f;
		hdrParams.exposure = 15.0f;
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtWorld2:
	{
		EnableLamps(false);
		EnableWater(true);
		EnableGrass(false);
		EnableGroundFog(false);
		EnableMagma(false);
		EnablePlanarRefl(false);

		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 1.7f;
		hdrParams.brightThreshold = 1.9f;
		hdrParams.gaussianScalar = 30.0f;
		hdrParams.exposure = 8.0f;
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtWorld3:	
	{
		EnableLamps(false);
		EnableWater(false);
		EnableGrass(false);
		EnableGroundFog(true);
		EnableMagma(false);
		EnablePlanarRefl(false);

		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 4.0f;
		hdrParams.brightThreshold = 4.5f;
		hdrParams.gaussianScalar = 20.0f;
		hdrParams.exposure = 3.0f;		
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtWorld4:
	{
		EnableLamps(false);
		EnableWater(false);
		EnableGrass(false);
		EnableGroundFog(false);
		EnableMagma(true);
		EnablePlanarRefl(false);

		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 1.9f;
		hdrParams.brightThreshold = 1.9f;
		hdrParams.gaussianScalar = 30.0f;
		hdrParams.exposure = 8.0f;
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtWorld5:
	{
		EnableLamps(false);
		EnableWater(false);
		EnableGrass(false);
		EnableGroundFog(false);
		EnableMagma(false);
		EnablePlanarRefl(true);

		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 1.1f;
		hdrParams.brightThreshold = 1.3f;
		hdrParams.gaussianScalar = 30.0f;
		hdrParams.exposure = 15.0f;
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtWorld6:
	{
		EnableLamps(false);
		EnableWater(false);
		EnableGrass(false);
		EnableGroundFog(true);
		EnableMagma(false);
		EnablePlanarRefl(false);

		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 1.7f;
		hdrParams.brightThreshold = 1.9f;
		hdrParams.gaussianScalar = 30.0f;
		hdrParams.exposure = 8.0f;
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtGarage:
	{
		EnableLamp(true, 0, 20.0f);
		EnableLamp(true, 1, 20.0f);
		EnableLamp(false, 2, 20.0f);
		EnableWater(false);
		EnableGrass(false);
		EnableGroundFog(false);
		EnableMagma(false);
		EnablePlanarRefl(false);

		GraphManager::HDRParams hdrParams;

		//hdrParams.lumKey = 3.0f;
		//hdrParams.brightThreshold = 3.5f;
		//hdrParams.gaussianScalar = 20.0f;
		//hdrParams.exposure = 1.0f;

		//hdrParams.lumKey = 2.5f;
		//hdrParams.brightThreshold = 4.0f;
		//hdrParams.gaussianScalar = 40.0f;
		//hdrParams.exposure = 3.5f;

		hdrParams.lumKey = 2.0f;
		hdrParams.brightThreshold = 4.0f;
		hdrParams.gaussianScalar = 25.0f;
		hdrParams.exposure = 2.0f;

		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	case wtAngar:
	{
		EnableLamp(true, 0, 80.0f);
		EnableLamp(true, 1, 80.0f);
		EnableLamp(true, 2, 100.0f);
		EnableWater(false);
		EnableGrass(false);
		EnableGroundFog(false);
		EnableMagma(false);
		EnablePlanarRefl(false);

		GraphManager::HDRParams hdrParams;
		hdrParams.lumKey = 3.0f;
		hdrParams.brightThreshold = 3.5f;
		hdrParams.gaussianScalar = 20.0f;
		hdrParams.exposure = 5.0f;
		hdrParams.colorCorrection = D3DXVECTOR2(1.0f, 0.0f);
		GetGraph()->SetHDRParams(hdrParams);
		break;
	}

	}

	ApplyCloudColor();
}

GraphManager* Environment::GetGraph()
{
	return _world->GetGraph();
}

CameraManager* Environment::GetCamera()
{
	return _world->GetCamera();
}

void Environment::ApplyQuality()
{
	SetGraphOption(GraphManager::goBumpMap, true);
	SetGraphOption(GraphManager::goRefr, true);
	SetGraphOption(GraphManager::goPixelLighting, true);
	SetGraphOption(GraphManager::goRefl, true);
	SetGraphOption(GraphManager::goTrueRefl, true);
	SetGraphOption(GraphManager::goShadow, true);

	GetGraph()->SetFiltering(cFilteringLevel[_filtering]);
	GetGraph()->SetMultisampling(cMultisamplingLevel[_multisampling]);

	ApplyWheater();
	ApplyWorldType();
}

void Environment::StartScene()
{
	if (_startScene)
		return;
	_startScene = true;

	ApplyRain();
}

void Environment::ReleaseScene()
{
	if (!_startScene)
		return;
	_startScene = false;

	FreeRain();
}

void Environment::ProcessScene(float dt)
{
	if (GetCamera() && _isoRain != (GetCamera()->GetStyle() == CameraManager::csIsometric))
		ApplyRain();

	if (_rain && GetCamera())
	{
		_rain->GetGameObj().SetPos(GetCamera()->GetPos());	
	}

	/*if (_rain && GetCamera())
	{
		graph::FxParticleSystem* fx =  (graph::FxParticleSystem*)&_rain->GetGameObj().GetGrActor().GetNodes().front();
		graph::FxFlowEmitter* em = (graph::FxFlowEmitter*)&fx->GetEmitters().front();
		graph::FxEmitter::ParticleDesc desc = em->GetParticleDesc();

		bool dir = _world->GetControl()->GetAsyncKey(VK_CONTROL);
		D3DXVECTOR3 vec = dir ? desc.startPos.GetMax() : desc.startPos.GetMin();

		if (_world->GetControl()->GetAsyncKey('W'))
			vec.x += 3.0f * dt;
		if (_world->GetControl()->GetAsyncKey('A'))
			vec.y += 3.0f * dt;		
		if (_world->GetControl()->GetAsyncKey('Z'))
			vec.z += 3.0f * dt;
		if (_world->GetControl()->GetAsyncKey('S'))
			vec.x -= 3.0f * dt;
		if (_world->GetControl()->GetAsyncKey('D'))
			vec.y -= 3.0f * dt;
		if (_world->GetControl()->GetAsyncKey('X'))
			vec.z -= 3.0f * dt;

		if (dir)
			desc.startPos.SetMax(vec);	
		else
			desc.startPos.SetMin(vec);		
		
		em->SetParticleDesc(desc);
	}*/
}

const D3DXVECTOR3& Environment::GetSunPos() const
{
	if (_sun)
		_sunPos = _sun->GetSource()->GetPos();

	return _sunPos;
}

void Environment::SetSunPos(const D3DXVECTOR3& value)
{
	_sunPos = value;

	if (_sun)
		_sun->GetSource()->SetPos(_sunPos);
}

const D3DXQUATERNION& Environment::GetSunRot() const
{
	if (_sun)
		_sunRot = _sun->GetSource()->GetRot();

	return _sunRot;
}

void Environment::SetSunRot(const D3DXQUATERNION& value)
{
	_sunRot = value;

	if (_sun)
		_sun->GetSource()->SetRot(_sunRot);
}

const D3DXVECTOR3& Environment::GetLampPos(int index) const
{
	return _lampPos[index];
}

void Environment::SetLampPos(const D3DXVECTOR3& value, int index)
{
	_lampPos[index] = value;

	if (_lamp[index])
		_lamp[index]->GetSource()->SetPos(value);
}

const D3DXQUATERNION& Environment::GetLampRot(int index) const
{
	return _lampRot[index];
}

void Environment::SetLampRot(const D3DXQUATERNION& value, int index)
{
	_lampRot[index] = value;

	if (_lamp[index])
		_lamp[index]->GetSource()->SetRot(value);
}

const D3DXCOLOR& Environment::GetLampColor(int index) const
{
	return _lampColor[index];
}

void Environment::SetLampColor(const D3DXCOLOR& value, int index)
{
	_lampColor[index] = value;

	if (_lamp[index])
		_lamp[index]->GetSource()->SetDiffuse(value);
}

void Environment::SwitchOnLamp(int index, bool value)
{
	_lampSwitchOn[index] = value;

	if (_lamp[index])
		_lamp[index]->SetEnable(value);
}

bool Environment::IsLampSwitchOn(int index)
{
	return _lampSwitchOn[index];
}

Environment::Wheater Environment::GetWheater() const
{
	return _wheater;
}

void Environment::SetWheater(Wheater value)
{
	if (_wheater != value)
	{
		_wheater = value;
		ApplyWheater();
	}
}

Environment::WorldType Environment::GetWorldType() const
{
	return _worldType;
}

void Environment::SetWorldType(WorldType value)
{
	if (_worldType != value)
	{		
		_worldType = value;
		ApplyWorldType();
	}
}

Environment::Quality Environment::GetShadowQuality() const
{
	return _shadowQuality;
}

void Environment::SetShadowQuality(Quality value)
{
	if (_shadowQuality != value)
	{
		_shadowQuality = value;
		ApplyQuality();
	}
}

bool Environment::IsShadowQualitySupported(Quality value)
{
	return IsGraphMapSupported(value, cShadowGraphMap, ARRAY_LENGTH(cShadowGraphMap));
}

Environment::Quality Environment::GetLightQuality() const
{
	return _lightQuality;
}

void Environment::SetLightQuality(Quality value)
{
	if (_lightQuality != value)
	{
		_lightQuality = value;
		ApplyQuality();
	}
}

bool Environment::IsLightQualitySupported(Quality value)
{
	return IsGraphMapSupported(value, cLightGraphMap, ARRAY_LENGTH(cLightGraphMap));
}

Environment::Quality Environment::GetPostEffQuality() const
{
	return _postEffQuality;
}

void Environment::SetPostEffQuality(Quality value)
{
	if (_postEffQuality != value)
	{
		_postEffQuality = value;
		ApplyQuality();
	}
}

bool Environment::IsPostEffQualitySupported(Quality value)
{
	return IsGraphMapSupported(value, cPostEffGraphMap, ARRAY_LENGTH(cPostEffGraphMap));
}

Environment::Quality Environment::GetEnvQuality() const
{
	return _envQuality;
}

void Environment::SetEnvQuality(Quality value)
{
	if (_envQuality != value)
	{
		_envQuality = value;
		ApplyQuality();
	}
}

bool Environment::IsEnvQualitySupported(Quality value)
{
	return IsGraphMapSupported(value, cEnvGraphMap, ARRAY_LENGTH(cEnvGraphMap));
}

Environment::Filtering Environment::GetFiltering() const
{
	return _filtering;
}

void Environment::SetFiltering(Filtering value)
{
	if (_filtering != value)
	{
		_filtering = value;
		ApplyQuality();
	}
}

bool Environment::IsFilteringSupported(Filtering value)
{
	return GetGraph()->IsFilteringSupported(cFilteringLevel[value]);
}

Environment::Multisampling Environment::GetMultisampling() const
{
	return _multisampling;
}

void Environment::SetMultisampling(Multisampling value)
{
	if (_multisampling != value)
	{
		_multisampling = value;
		ApplyQuality();
	}
}

bool Environment::IsMultisamplingSupported(Multisampling value)
{
	return GetGraph()->IsMultisamplingSupported(cMultisamplingLevel[value]);
}

void Environment::AutodetectQuality()
{
	for (int i = cQualityEnd - 2; i >= 0; --i)
	{
		Quality quality = (Quality)i;

		if (IsShadowQualitySupported(quality))
		{
			SetShadowQuality(quality);
			break;
		}
	}

	for (int i = cQualityEnd - 1; i >= 0; --i)
	{
		Quality quality = (Quality)i;

		if (IsLightQualitySupported(quality))
		{
			SetLightQuality(quality);
			break;
		}
	}

	for (int i = cQualityEnd - 1; i >= 0; --i)
	{
		Quality quality = (Quality)i;

		if (IsPostEffQualitySupported(quality))
		{
			SetPostEffQuality(quality);
			break;
		}
	}

	for (int i = cQualityEnd - 1; i >= 0; --i)
	{
		Quality quality = (Quality)i;

		if (IsEnvQualitySupported(quality))
		{
			SetEnvQuality(quality);
			break;
		}
	}

	//for (int i = emSamples4; i >= 0; --i)
	//{
	//	Multisampling multisampling = (Multisampling)i;
	//
	//	if (IsMultisamplingSupported(multisampling))
	//	{
	//		SetMultisampling(multisampling);
	//		break;
	//	}
	//}

	SetMultisampling(emNone);

	for (int i = efAnisotropic8; i >= 0; --i)
	{
		Filtering filtering = (Filtering)i;

		if (IsFilteringSupported(filtering))
		{
			SetFiltering(filtering);
			break;
		}
	}

	SetSyncFrameRate(sfrFixed);
}

Environment::SyncFrameRate Environment::GetSyncFrameRate()
{
	return _syncFrameRate;
}

void Environment::SetSyncFrameRate(SyncFrameRate value)
{
	_syncFrameRate = value;
}

float Environment::GetOrthoCameraFar() const
{
	if (_editMode)
		return 1000.0f;

	return 150.0f;
}

float Environment::GetPerspectiveCameraFar() const
{
	if (_editMode)
		return 1000.0f;

	switch (_wheater)
	{
	case ewFair:
		return 120.0f;		

	case ewNight:
		return 120.0f;
		
	case ewClody:
		return 120.0f;
		
	case ewRainy:
		return 100.0f;

	case ewGarage:
		return 20.0f;

	case ewAngar:
		return 130.0f;
	}

	return 100.0f;
}

bool Environment::GetGUIMode() const
{
	return _world->GetGraph()->GetGUIMode();
}

void Environment::SetGUIMode(bool value)
{
	_world->GetGraph()->SetGUIMode(value);
}

bool Environment::GetEditMode() const
{
	return _editMode;
}

void Environment::SetEditMode(bool value)
{
	if (_editMode != value)
	{
		_editMode = value;
		ApplyWheater();
	}
}

}

}