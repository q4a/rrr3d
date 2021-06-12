#include "stdafx.h"

#include "graph\\MappingShaders.h"

namespace r3d
{

namespace graph
{

const lsl::string LightShader::cLightTypeStr[LightShader::cLightTypeEnd] = {"LIGHT_TYPE_POINT", "LIGHT_TYPE_SPOT", "LIGHT_TYPE_DIR"};
const lsl::string LightShader::cLightPropStr[LightShader::cLightPropEnd] = {"", "SHADOW"};
const lsl::string LightShader::cMyParamStr[cMyParamEnd] = {"numLights", "glAmbient", "ambLight", "diffLight", "specLight", "colorMat", "specMat", "specPower", "texDiffK", "worldMat", "worldViewMat", "wvpMat", "viewPos", "diffTex", "shadowTex", "lightPos", "lightDir", "spotParams", "fogParams", "fogColor", "reflectivity", "envTex", "alphaBlendColor", "normTex", "mWorldViewProj", "refrTex", "vScene", "reflTex"};




LightShader::LightShader(): _curLight(0), _fogColor(clrWhite), _viewPos(NullVector), _texDiffK(1.0f), _paramsLight(D3DLIGHT_FORCE_DWORD), _paramsShadow(false)
{
	for (int i = 0; i < cLightTypeEnd; ++i)
		for (int j = 0; j < cLightPropEnd; ++j)
		{
			Macros macros;
			if (!cLightTypeStr[i].empty())
				macros.push_back(Macro(cLightTypeStr[i], ""));
			if (!cLightPropStr[j].empty())
				macros.push_back(Macro(cLightPropStr[j], ""));

			_lightMacro[i][j] = &GetMacros().Add(macros);
			_lightMacro[i][j]->AddRef();
		}

	//Макрос будет определяться динамически, обнуляем макрос по умолчанию чтобы избежать случайного применения
	SetMacro(0);
}

LightShader::~LightShader()
{
	for (int i = 0; i < cLightTypeEnd; ++i)
		for (int j = 0; j < cLightPropEnd; ++j)
			lsl::SafeRelease(_lightMacro[i][j]);
}

void LightShader::InvalidateParams(D3DLIGHTTYPE value, bool shadow)
{
	if (_paramsLight == value && GetCurMacro() && _paramsShadow == shadow)
		return;

	UnApplyTech();

	_paramsLight = value;
	_paramsShadow = shadow;

	switch (value)
	{
	case D3DLIGHT_POINT:
		ApplyTech("", shadow ? _lightMacro[ltPoint][lpShadow] : _lightMacro[ltPoint][lpNoShadow]);		
		break;

	case D3DLIGHT_DIRECTIONAL:
		ApplyTech("", shadow ? _lightMacro[ltDir][lpShadow] : _lightMacro[ltDir][lpNoShadow]);		
		break;

	case D3DLIGHT_SPOT:
		ApplyTech("", shadow ? _lightMacro[ltSpot][lpShadow] : _lightMacro[ltSpot][lpNoShadow]);		
		break;

	default:
		LSL_ASSERT(false);
	}

	for (int i = 0; i < cMyParamEnd; ++i)
		_params[i] = GetParam(cMyParamStr[i]);
}

void LightShader::DoFree()
{
	_paramsLight = D3DLIGHT_FORCE_DWORD;

	Shader::DoFree();
}

void LightShader::DoBeginDraw(Engine& engine)
{
	unsigned lightCnt = engine.GetContext().GetLights().size();

	LSL_ASSERT(lightCnt > _curLight);

	//Первый проход
	bool firstPass = _curLight == 0;
	//Послдений проход
	bool lastPass = _curLight >= lightCnt - 1;
	//
	_fogColor = D3DXCOLOR(engine.GetContext().GetRenderState(rsFogColor));
	engine.GetContext().SetRenderState(rsFogColor, _fogColor / static_cast<float>(lightCnt));
	
	const LightCI& light = engine.GetContext().GetLight(_curLight);
	bool shadow = engine.GetContext().GetLightShadow() && light.GetDesc().shadowMap;
	D3DLIGHTTYPE lightType = light.GetDesc().type;

	InvalidateParams(lightType, shadow);

	switch (lightType)
	{
	case D3DLIGHT_POINT:
		SetParam(_params[lightPos], light.GetDesc().pos);
		SetParam(_params[lightDir], light.GetDesc().dir);		
		break;

	case D3DLIGHT_DIRECTIONAL:
		SetParam(_params[lightPos], light.GetDesc().pos);
		SetParam(_params[lightDir], light.GetDesc().dir);
		break;

	case D3DLIGHT_SPOT:		
	{
		SetParam(_params[lightPos], light.GetDesc().pos);
		SetParam(_params[lightDir], light.GetDesc().dir);

		D3DXVECTOR4 spotParamsVec(light.GetDesc().falloff, light.GetDesc().phi, light.GetDesc().theta, light.GetDesc().range);
		SetParam(_params[spotParams], spotParamsVec);
		break;
	}

	default:
		LSL_ASSERT(false);
	}

	SetParam(_params[numLights], static_cast<float>(lightCnt));
	SetParam(_params[glAmbient], D3DXCOLOR(engine.GetContext().GetRenderState(rsAmbient)));

	SetParam(_params[ambLight], light.GetDesc().ambient);
	SetParam(_params[diffLight], light.GetDesc().diffuse);
	SetParam(_params[specLight], light.GetDesc().specular);
	
	D3DMATERIAL9 d3dMat;
	engine.GetDriver().GetDevice()->GetMaterial(&d3dMat);

	SetParam(_params[colorMat], D3DXCOLOR(d3dMat.Diffuse));
	SetParam(_params[specMat], D3DXCOLOR(d3dMat.Specular));
	SetParam(_params[specPower], d3dMat.Power);
	SetParam(_params[texDiffK], _texDiffK * engine.GetContext().GetTexDiffK());

	SetParam(_params[worldMat], engine.GetContext().GetWorldMat());
	SetParam(_params[worldViewMat], engine.GetContext().GetCamera().GetTransform(CameraCI::ctWorldView));
	SetParam(_params[wvpMat], engine.GetContext().GetCamera().GetWVP());
	SetParam(_params[viewPos], _viewPos);

	//register(s0)
	SetTexParam(_params[diffTex], engine.GetContext().GetTexture(0));

	if (shadow)
		SetTexParam(_params[shadowTex], light.GetDesc().shadowMap->GetTex());

	if (_curLight == 1)
	{
		engine.GetContext().SetRenderState(rsAlphaBlendEnable, true);
		engine.GetContext().SetRenderState(rsDestBlend, D3DBLEND_ONE);
		engine.GetContext().SetRenderState(rsSrcBlend, D3DBLEND_ONE);
	}

	D3DXVECTOR3 fogParamsVec = D3DXVECTOR3(0, 1, (float)engine.GetContext().GetRenderState(rsFogEnable));
	if (fogParamsVec.z != 0)
	{
		DWORD dwVal = engine.GetContext().GetRenderState(rsFogStart);
		fogParamsVec.x = *(float*)(&dwVal);

		dwVal = engine.GetContext().GetRenderState(rsFogEnd);
		fogParamsVec.y = *(float*)(&dwVal);

		D3DXCOLOR fogColorVec = D3DXCOLOR(engine.GetContext().GetRenderState(rsFogColor));
		SetParam(_params[fogColor], fogColorVec);
	}

	SetParam(_params[fogParams], fogParamsVec);
}

bool LightShader::DoEndDraw(Engine& engine, bool nextPass)
{
	++_curLight;

	unsigned lightCnt = engine.GetContext().GetLights().size();

	//Нужен ли следующих проход
	bool needNextPass = nextPass &&  lightCnt > _curLight;

	engine.GetContext().SetRenderState(rsFogColor, _fogColor);

	if (!needNextPass)
	{
		if (_curLight > 0)
		{
			engine.GetContext().RestoreRenderState(rsAlphaBlendEnable);
			engine.GetContext().RestoreRenderState(rsDestBlend);
			engine.GetContext().RestoreRenderState(rsSrcBlend);
		}

		_curLight = 0;
	}

	return !needNextPass;
}

const D3DXVECTOR3& LightShader::GetViewPos() const
{
	return _viewPos;
}

void LightShader::SetViewPos(const D3DXVECTOR3& value)
{
	_viewPos = value;
}

float LightShader::GetTexDiffK() const
{
	return _texDiffK;
}

void LightShader::SetTexDiffK(float value)
{
	_texDiffK = value;
}




ReflMappShader::ReflMappShader(): _reflectivity(0.4f)
{
	SetTech("techReflMapp");
}

void ReflMappShader::DoBeginDraw(Engine& engine)
{
	_MyBase::DoBeginDraw(engine);

	SetParam(_params[reflectivity], _reflectivity);	
	SetParam(_params[alphaBlendColor], D3DXCOLOR(engine.GetContext().GetTextureStageState(0, tssConstant)));

	if (GetReflTex())
		SetTexParam(_params[envTex], GetReflTex()->GetTex());	
}

graph::TexCubeResource* ReflMappShader::GetReflTex()
{
	return lsl::StaticCast<graph::TexCubeResource*>(GetTexture("envTex"));
}

void ReflMappShader::SetReflTex(graph::TexCubeResource* value)
{
	SetTexture("envTex", value);
}

float ReflMappShader::GetReflectivity() const
{
	return _reflectivity;
}

void ReflMappShader::SetReflectivity(float value)
{
	_reflectivity = value;
}




ReflBumbMappShader::ReflBumbMappShader()
{
	SetTech("techReflMapp");
}

void ReflBumbMappShader::DoBeginDraw(Engine& engine)
{
	_MyBase::DoBeginDraw(engine);

	SetParam(_params[reflectivity], 0.4f);	
	SetParam(_params[alphaBlendColor], D3DXCOLOR(engine.GetContext().GetTextureStageState(0, tssConstant)));

	if (GetReflTex())
		SetTexParam(_params[envTex], GetReflTex()->GetTex());	

	SetTexParam(_params[normTex], engine.GetContext().GetTexture(1));
}

graph::TexCubeResource* ReflBumbMappShader::GetReflTex()
{
	return lsl::StaticCast<graph::TexCubeResource*>(GetTexture("envTex"));
}

void ReflBumbMappShader::SetReflTex(graph::TexCubeResource* value)
{
	SetTexture("envTex", value);
}




BumpMapShader::BumpMapShader()
{
	SetTech("techBumpMap");
}

void BumpMapShader::DoBeginDraw(Engine& engine)
{
	_MyBase::DoBeginDraw(engine);

	SetTexParam(_params[normTex], engine.GetContext().GetTexture(1));
}




RefrShader::RefrShader()
{
	SetTech("techRefract");
}

void RefrShader::DoBeginDraw(Engine& engine)
{
	SetValueDir("mWorldViewProj", engine.GetContext().GetCamera().GetWVP());
	SetTextureDir("refrTex", engine.GetContext().GetTexture(0));
	SetValueDir("vScene", (1.0f - engine.GetContext().GetFrame()) * 1.0f);// (1.0f - engine.GetContext().frame) * 0.1f);
}




PlanarReflMappShader::PlanarReflMappShader()
{
	SetTech("techReflMapp");
}

void PlanarReflMappShader::DoBeginDraw(Engine& engine)
{
	_MyBase::DoBeginDraw(engine);

	SetParam(_params[reflectivity], 0.4f);

	if (GetReflTex())
		SetTexParam(_params[reflTex], GetReflTex()->GetTex());	
}

graph::Tex2DResource* PlanarReflMappShader::GetReflTex()
{
	return lsl::StaticCast<graph::Tex2DResource*>(GetTexture("reflTex"));
}

void PlanarReflMappShader::SetReflTex(graph::Tex2DResource* value)
{
	SetTexture("reflTex", value);
}

}

}