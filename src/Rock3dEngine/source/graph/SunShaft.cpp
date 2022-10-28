#include "stdafx.h"

#include "graph\\SunShaft.h"

namespace r3d
{

namespace graph
{

SunShaftRender::SunShaftRender()
{
	int blurK[cShaftsTexNum] = {2, 4};
	for (int i = 0; i < cShaftsTexNum; ++i)
	{
		Tex2DResource& tex = _blurTex.Add();
		tex.SetDynamic(true);
		tex.SetUsage(D3DUSAGE_RENDERTARGET);
		tex.GetOrCreateData()->SetFormat(D3DFMT_X8R8G8B8);
		tex.GetData()->SetWidth(cShaftsTexSizeX / blurK[i]);
		tex.GetData()->SetHeight(cShaftsTexSizeY / blurK[i]);

		_blurVec.push_back(&tex);
	}

	shader.SetTexture("colorBlurTex", _blurVec[0]);
}

SunShaftRender::~SunShaftRender()
{
	shader.ClearTextures();
}

void SunShaftRender::Render(Engine& engine)
{
	if (engine.GetContext().GetLights().empty())
		return;

	for (Textures::iterator iter = _blurTex.begin(); iter != _blurTex.end(); ++iter)
		(*iter)->Init(engine);

	//Подготовка шафтов
	IDirect3DSurface9* blurSurf;
	_blurVec[0]->GetTex()->GetSurfaceLevel(0, &blurSurf);
	engine.GetDriver().GetDevice()->SetRenderTarget(0, blurSurf);
	blurSurf->Release();

	shader.Apply(engine, "techPrepareShafts", 0);
	DrawScreenQuad(engine);
	shader.UnApply(engine);

	//Ping Pong текстуры шафтов
	IDirect3DTexture9* swapingTex[2] = {_blurVec[0]->GetTex(), _blurVec[1]->GetTex()};
	engine.GetContext().SetSamplerState(0, ssMagFilter, D3DTEXF_LINEAR);
	engine.GetContext().SetSamplerState(0, ssMinFilter, D3DTEXF_LINEAR);
	engine.GetContext().SetSamplerState(0, ssMipFilter, D3DTEXF_NONE);

	for (int i = 0; i < 8; ++i)
	{
		std::swap(swapingTex[0], swapingTex[1]);
		IDirect3DSurface9* swapingSurf;
		swapingTex[0]->GetSurfaceLevel(0, &swapingSurf);

		engine.GetDriver().GetDevice()->SetRenderTarget(0, swapingSurf);
		engine.GetContext().SetTexture(0, swapingTex[1]);
		swapingSurf->Release();

		DrawScreenQuad(engine);
	}

	engine.GetContext().SetTexture(0, 0);
	engine.GetContext().RestoreSamplerState(0, ssMagFilter);
	engine.GetContext().RestoreSamplerState(0, ssMinFilter);
	engine.GetContext().RestoreSamplerState(0, ssMipFilter);

	glm::vec4 sunPos(Vec3GlmToDx(_sunPos), 1.0f);
	D3DXVec4Transform(&sunPos, &sunPos, &engine.GetContext().GetCamera().GetViewProj());
	sunPos.x /= sunPos.w;
	sunPos.y /= sunPos.w;
	sunPos.z /= sunPos.w; //sunPos.z = 1.0f;
	sunPos.x *= 0.5f;
	sunPos.y *= 0.5f;

	//Стд. техника учитывающая напрявление ист. света
	//glm::vec3 posNorm;
	//posNorm = glm::normalize(engine.GetLight()->GetDesc().pos);
	//sunPos.w = glm::dot(posNorm, engine.GetCamera()->GetDesc().dir);
	//
	if (sunPos.w > 0.0f)
	{
		//Не учитвает направление ист. света
		//glm::vec3 posNorm;
		//posNorm = glm::normalize(glm::vec3(sunPos));
		//sunPos.w = glm::dot(posNorm, engine.GetCamera()->GetDesc().up);
		//
		//Без затухания
		sunPos.w = 1.0f;
	}
	else
		sunPos.w = 0.0f;
	shader.SetValueDir("sunPos", sunPos);

	ApplyRT(engine, RtFlags(0, 0));

	shader.Apply(engine, "techGenShafts", 0);
	DrawScreenQuad(engine);
	shader.UnApply(engine);

	UnApplyRT(engine);
}

Tex2DResource* SunShaftRender::GetColorTex()
{
	return static_cast<Tex2DResource*>(shader.GetTexture("colorTex"));
}

void SunShaftRender::SetColorTex(Tex2DResource* value)
{
	shader.SetTexture("colorTex", value);
}

Tex2DResource* SunShaftRender::GetDepthTex()
{
	return lsl::StaticCast<Tex2DResource*>(shader.GetTexture("depthTex"));
}

void SunShaftRender::SetDepthTex(Tex2DResource* value)
{
	shader.SetTexture("depthTex", value);
}

const glm::vec3& SunShaftRender::GetSunPos() const
{
	return _sunPos;
}

void SunShaftRender::SetSunPos(const glm::vec3& value)
{
	_sunPos = value;
}

}

}