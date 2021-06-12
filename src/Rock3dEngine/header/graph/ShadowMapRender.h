#ifndef SHADOW_MAP_RENDER
#define SHADOW_MAP_RENDER

#include "MaterialLibrary.h"
#include "DepthMap.h"
#include "RenderToTexture.h"
#include <vector>

namespace r3d
{

namespace graph
{

class ShadowMapShader: public Shader
{
protected:
	virtual void DoBeginDraw(Engine& engine);
public:
	D3DXMATRIX mTexScale;
	D3DXMATRIX shadowViewProj;
};

//Выисление карты освещенности, которая представляет собой проекционную текстуру на текстуру сцены. Т.е. накладывается постпроцессом. Содержит в себе степень освещенности (тени прикрытые диффузом) в помежутке 0..1.
class ShadowMapRender: public GraphObjRender<Tex2DResource>
{
private:
	typedef GraphObjRender<Tex2DResource> _MyBase;	
public:
	static const unsigned cShadowMapSize;

	typedef lsl::ResourceCollection<Tex2DResource, void, void, void> Textures;
private:
	unsigned _numSplits;
	float _splitSchemeLambda;
	bool _disableCropLight;
	float _maxFar;

	graph::DepthStencilSurfaceResource* _depthSurface;
	IDirect3DSurface9* _lastDepthSurface;

	std::vector<float> _splitDistances;
	std::vector<float> _lightDist;
	std::vector<D3DXMATRIX> _splitLightProjMat;

	//
	RtFlags _beginFlags;
	CameraCI _myCamera;
	D3DVIEWPORT9 _oldViewPort;
	unsigned _curNumSplit;

	bool _beginShadowCaster;
	bool _beginShadowMapp;

	void CalculateSplitDistances(const CameraCI& camera);
	void ComputeCropMatrix(unsigned numSplit, const LightCI& light, const Frustum::Corners& pCorners);
	void CalcSplitScheme(const CameraCI& camera, const LightCI& light);
protected:
	virtual Tex2DResource* CreateRT();

	void DoBeginShadowCaster(Engine& engine, unsigned numSplit);
	void DoEndShadowCaster(Engine& engine);
public:
	Textures _shadowMaps;
	std::vector<Tex2DResource*> _shadowVec;

	ShadowMapRender();
	virtual ~ShadowMapRender();

	//Рендер объектов которые создают тень
	void BeginShadowCaster(Engine& engine);
	bool EndShadowCaster(Engine& engine, bool nextPass);
	
	//Рендер объектов которые принимают тень
	//Начало прохода
	void BeginShadowMapp(Engine& engine);
	bool EndShadowMapp(Engine& engine, bool nextPass);	

	//Начать рендер эффекта
	virtual void BeginRT(Engine& engine, const RtFlags& flags);
	virtual void EndRT(Engine& engine);

	void RenderDebug(Engine& engine);

	unsigned GetNumSplits() const;
	void SetNumSplits(unsigned value);

	float GetSplitSchemeLambda() const;
	void SetSplitSchemeLambda(float value);

	bool GetDisableCropLight() const;
	void SetDisableCropLight(bool value);

	float GetMaxFar() const;
	void SetMaxFar(float value);

	DepthMapRender depthRender;
	ShadowMapShader shader;
	unsigned iLight;
};

//Комбинирование карт освещенности в одну
class CombineLightMap: public PostEffRender<Tex2DResource>
{
public:
	typedef std::list<Tex2DResource*> LightMapList;
private:
	LightMapList _lightMapList;
public:
	virtual ~CombineLightMap();

	void InsertLightMap(Tex2DResource* value);
	void RemoveLightMap(Tex2DResource* value);
	void ClearLightMapList();

	const LightMapList& GetLightMapList() const;

	virtual void Render(Engine& engine);

	Shader shader;
};

//Наложение карты теней со стандартным освещением на объекты с фикс. рендером
class MappingLightMap: public PostEffRender<Tex2DResource>
{
private:
	typedef PostEffRender<Tex2DResource> _MyBase;
public:
	//Добавить также наложение глобального амбиента
	virtual void Render(Engine& engine);

	Tex2DResource* GetColorTex();
	void SetColorTex(Tex2DResource* value);
	Tex2DResource* GetLightMapTex();
	void SetLightMapTex(Tex2DResource* value);

	Shader shader;
};

}

}

#endif