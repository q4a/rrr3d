#ifndef R3D_GRAPH_CONTEXTINFO
#define R3D_GRAPH_CONTEXTINFO

#include "VideoResource.h"
#include "driver\\RenderDriver.h"
#include "r3dMath.h"

#include <map>
#include <vector>
#include <deque>
#include <stack>

namespace r3d
{

namespace graph
{

class Engine;

template<class _State, class _Value, _Value _defValue[]> class StateManager
{
private:
	typedef std::map<_State, _Value> _States;
public:
	typedef typename _States::iterator iterator;	
private:
	_States _states;
public:
	_Value Get(_State state) const;
	void Set(_State state, _Value value);
	void Restore(_State state);
	void Reset();

	iterator begin();
	iterator end();
};

template<class _State, class _Value> class StateStack
{
private:
	struct ValueRef
	{
		ValueRef(const _Value& mValue): value(mValue), refCnt(0) {}
		~ValueRef() {LSL_ASSERT(refCnt == 0);}

		void AddRef()
		{
			++refCnt;
		}
		unsigned Release()
		{
			LSL_ASSERT(refCnt > 0);

			return --refCnt;
		}

		_Value value;
		unsigned refCnt;
	};

	typedef std::stack<ValueRef> ValueStatck;
	typedef std::map<_State, ValueStatck> States;
private:
	States _states;
public:
	~StateStack()
	{
		LSL_ASSERT(_states.empty());
	}

	bool Push(const _State& state, const _Value& value)
	{
		States::iterator iter = _states.find(state);
		if (iter == _states.end())		
			iter = _states.insert(_states.end(), States::value_type(state, ValueStatck()));

		bool res = !iter->second.empty() && iter->second.top().value == value;
		if (res)
			iter->second.top().AddRef();
		else
		{
			iter->second.push(ValueRef(value));
			iter->second.top().AddRef();
		}

		return !res;
	}
	bool Pop(const _State& state)
	{
		States::iterator iter = _states.find(state);
		
		LSL_ASSERT(iter != _states.end());

		bool res = iter->second.top().Release() == 0;
		if (res)
		{
			iter->second.pop();
			if (iter->second.empty())
				_states.erase(iter);
		}

		return res;
	}

	const _Value& Back(const _State& state) const
	{
		States::const_iterator iter = _states.find(state);

		LSL_ASSERT(iter != _states.end());

		return iter->second.top().value;
	}

	const _Value* End(const _State& state) const
	{
		States::const_iterator iter = _states.find(state);

		if (iter == _states.end())
			return 0;

		return iter->second.empty() ? 0 : &(iter->second.top().value);
	}
};

enum CombTransformStateType {ctsWorldView, ctsViewProj, ctsWVP, COMB_TRANSFORM_STATE_TYPE};

//Описывает способность к рендеру, а также пространство которое объект занимает в мировых координатах
class Renderable: public virtual lsl::Object
{
public:
	virtual void Render(Engine& engine) = 0;
};

//Экранный, внеэкранный буффер
class RenderBuffer: public virtual lsl::Object
{	
public:
	virtual void Render(Engine& engine, IDirect3DSurface9* backBuffer, IDirect3DSurface9* dsSurface) = 0;
};

struct MaterialDesc
{
	D3DXCOLOR diffuse;
    D3DXCOLOR ambient;
    D3DXCOLOR specular;
    D3DXCOLOR emissive;
    float power;
};

enum CameraStyle
{
	csPerspective,
	csOrtho,
	csViewPort,
	csViewPortInv
};

struct CameraDesc
{
	CameraDesc();

	float aspect;
	float fov;
	float nearDist;
	float farDist;
	//
	CameraStyle style;
	//ширина плоскости на которую проецируется изображение для csOrtho, csViewPort матриц
	float width;

	D3DXVECTOR3 pos;
	D3DXVECTOR3 dir;
	D3DXVECTOR3 up;
};

struct LightDesc
{
	LightDesc()
	{
		type = D3DLIGHT_SPOT;
		ambient = clrBlack;
		diffuse = clrWhite;
		specular = clrWhite;
		
		aspect = 1.0f;
		nearDist = 1.0f;
		range = 100.0f;
		falloff = 1.0f;
		attenuation0 = 1;
		attenuation1 = 0;
		attenuation2 = 0;
		phi = D3DX_PI/2.0f;
		theta = D3DX_PI/4.0f;		

		pos = NullVector;
		dir = XVector;
		up = ZVector;
		shadowMap = 0;
	}

	D3DLIGHTTYPE type;
	D3DXCOLOR ambient;
    D3DXCOLOR diffuse;
    D3DXCOLOR specular;
	
	//Соотношение сторон фрустума отдельного направления точечного, направленного источника света. Для конусного игнорируется.
	float aspect;
	float nearDist;
	float range;
    float falloff;
    float attenuation0;
    float attenuation1;
    float attenuation2;
    float theta;
    float phi;
	
	D3DXVECTOR3 pos;
    D3DXVECTOR3 dir;
	D3DXVECTOR3 up;

	//Карта теней в пространстве текущей камеры
	Tex2DResource* shadowMap;	
};

class ContextInfo;

class CameraCI: public virtual lsl::Object
{
	friend ContextInfo;
public:
	enum Transform {ctView = 0, ctProj, ctWorldView, ctViewProj, ctWVP, cTransformEnd};

	static public D3DXVECTOR2 ViewToProj(const D3DXVECTOR2& coord, const D3DXVECTOR2& viewSize);
	static public D3DXVECTOR2 ProjToView(const D3DXVECTOR2& coord, const D3DXVECTOR2& viewSize);
private:
	CameraDesc _desc;
	
	D3DXMATRIX _worldMat;
	mutable D3DXMATRIX _matrices[cTransformEnd];
	mutable D3DXMATRIX _invMatrices[cTransformEnd];
	mutable std::bitset<cTransformEnd> _matChanged;
	mutable std::bitset<cTransformEnd> _invMatChanged;
	//
	mutable Frustum _frustum;
	mutable bool _frustChanged;

	unsigned _idState;

	void CalcProjPerspective(D3DXMATRIX& mat) const;

	void StateChanged();
	void WorldMatChanged(const D3DXMATRIX& worldMat);
	void ProjMatChanged();
	void DescChanged();
public:
	CameraCI();

	//Уникальный идентификатор состояния
	unsigned IdState() const;

	bool ComputeZBounds(const AABB& aabb, float& minZ, float& maxZ) const;
	void AdjustNearFarPlane(const AABB& aabb, float minNear, float maxFar);	

	void GetProjPerspective(D3DXMATRIX& mat) const;
	void GetViewProjPerspective(D3DXMATRIX& mat) const;
	void GetWVPPerspective(D3DXMATRIX& mat) const;
	void SetProjMat(const D3DXMATRIX& value);

	D3DXVECTOR3 ScreenToWorld(const D3DXVECTOR2& coord, float z, const D3DXVECTOR2& viewSize) const;
	D3DXVECTOR2 WorldToScreen(const D3DXVECTOR3& coord, const D3DXVECTOR2& viewSize) const;

	const CameraDesc& GetDesc() const;
	void SetDesc(const CameraDesc& value);

	const D3DXMATRIX& GetTransform(Transform transform) const;
	const D3DXMATRIX& GetInvTransform(Transform transform) const;
	const Frustum& GetFrustum() const;

	const D3DXMATRIX& GetView() const;
	const D3DXMATRIX& GetProj() const;
	const D3DXMATRIX& GetViewProj() const;
	const D3DXMATRIX& GetWVP() const;
	//
	const D3DXMATRIX& GetInvView() const;
	const D3DXMATRIX& GetInvProj() const;
	const D3DXMATRIX& GetInvViewProj() const;
	const D3DXMATRIX& GetInvWVP() const;	
};

class LightCI: public lsl::Object
{
	friend ContextInfo;
private:
	LightDesc _desc;
	bool _enable;
	CameraCI _camera;

	bool _changed;
	ContextInfo* _owner;
	unsigned _id;
public:
	LightCI();

	void AdjustNearFarPlane(const AABB& aabb, float minNear, float maxFar);
	//Данные изменены с момента прошлого прохода
	bool IsChanged() const;

	const LightDesc& GetDesc() const;
	void SetDesc(const LightDesc& value);

	const CameraCI& GetCamera() const;	
};

class BaseShader
{
public:
	virtual void BeginDraw(Engine& engine) = 0;
	virtual bool EndDraw(Engine& engine, bool nextPass) { return true;}
};

class ContextInfo
{
public:
	static const unsigned cMaxTexSamplers = 8;
	
	static const TransformStateType ContextInfo::cTexTransform[8];
	static DWORD defaultRenderStates[RENDER_STATE_END];
	static DWORD defaultSamplerStates[SAMPLER_STATE_END];
	static DWORD defaultTextureStageStates[TEXTURE_STAGE_STATE_END];

	typedef std::stack<CameraCI*> CameraStack;
	typedef std::vector<LightCI*> Lights;
	typedef std::deque<BaseShader*> ShaderStack;

	typedef StateStack<LightCI*, bool> LightEnableState;

	static DWORD GetDefTextureStageState(int stage, TextureStageState state);

	static const int cMeshIdIgnore = 1 << 31;
private:
	RenderDriver* _driver;

	D3DXMATRIX _worldMat;	

	std::vector<D3DXMATRIX> _textureMatStack[cMaxTexSamplers];
	IDirect3DBaseTexture9* _textures[cMaxTexSamplers];	
	int _maxTextureStage;

	MaterialDesc _material;
	DWORD _renderStates[RENDER_STATE_END];	
	DWORD _samplerStates[cMaxTexSamplers][SAMPLER_STATE_END];
	DWORD _textureStageStates[cMaxTexSamplers][TEXTURE_STAGE_STATE_END];

	bool _enableShadow;
	float _texDiffK;
	bool _invertingCullFace;
	bool _ignoreMaterial;	

	CameraStack _cameraStack;
	ShaderStack _shaderStack;

	Lights _lightList;
	LightEnableState _lightEnable;
	Lights::const_iterator _lastLight;

	std::stack<float> _frameStack;
	float _cullOpacity;
	D3DXCOLOR _color;
	int _meshId;

	DWORD InvertCullFace(DWORD curFace);
	void SetCamera(CameraCI* camera);

	void SetLight(LightCI* light, DWORD lightIndex);
	void SetLightEnable(DWORD lightIndex, bool value);
public:
	ContextInfo(RenderDriver* driver);
	~ContextInfo();

	void SetDefaults();

	void BeginDraw();
	void EndDraw(bool nextPass);

	void ApplyCamera(CameraCI* camera);
	void UnApplyCamera(CameraCI* camera);

	void AddLight(LightCI* value);
	void RemoveLight(LightCI* value);
	//
	bool GetLightEnable(LightCI* light) const;
	void SetLightEnable(LightCI* light, bool value);
	void RestoreLightEnable(LightCI* value);
	//
	bool GetLightShadow() const;
	void SetLightShadow(bool value);
	void RestoreLightShadow();
	//
	float GetTexDiffK() const;
	void SetTexDiffK(float value);

	//Одновременно несколько шейдеров работать немогут, поэтому этот менеджер следит за тем, чтобы только первый из установленных шейдеров был активен
	//Возвращает true если шейдер дальше можно использовать
	void PushShader(BaseShader* value);
	void PopShader(BaseShader* value);
	bool IsShaderActive() const;

	const D3DXMATRIX& GetWorldMat() const;
	void SetWorldMat(const D3DXMATRIX& value);

	void PushTextureTransform(int stage, const D3DXMATRIX& value);
	void PopTextureTransform(int stage);

	const MaterialDesc& GetMaterial() const;
	void SetMaterial(const MaterialDesc& value);

	DWORD GetRenderState(RenderState type);
	void SetRenderState(RenderState type, DWORD value);
	void RestoreRenderState(RenderState type);

	IDirect3DBaseTexture9* GetTexture(DWORD sampler);
	void SetTexture(DWORD sampler, IDirect3DBaseTexture9* value);

	DWORD GetSamplerState(DWORD sampler, SamplerState type);
	void SetSamplerState(DWORD sampler, SamplerState type, DWORD value);
	void RestoreSamplerState(DWORD sampler, SamplerState type);

	DWORD GetTextureStageState(DWORD sampler, TextureStageState type);
	void SetTextureStageState(DWORD sampler, TextureStageState type, DWORD value);
	void RestoreTextureStageState(DWORD sampler, TextureStageState type);

	bool GetInvertingCullFace() const;
	void SetInvertingCullFace(bool value);

	bool GetIgnoreMaterial();
	void SetIgnoreMaterial(bool value);	

	const ShaderStack& GetShaderStack() const;

	const CameraCI& GetCamera() const;
	const LightCI& GetLight(unsigned id) const;
	const Lights& GetLights() const;
	BaseShader& GetShader();

	//текущий прогресс анимации в промежутке [0..1](начало...конец), выход за этот промежуток возможен и поведение определяется самим узлом сцены (обычно затайливание)
	float GetFrame() const;
	void PushFrame(float value);
	void PopFrame();

	//частный случай для подстройки прозрачности при отсечении
	float GetCullOpacity() const;
	void SetCullOpacity(float value);
	void RestoreCullOpacity();
	bool IsCullOpacity() const;

	const D3DXCOLOR& GetColor() const;
	void SetColor(const D3DXCOLOR& value);

	int GetMeshId() const;
	void SetMeshId(int value);

	bool IsNight() const;
};




template<class _State, class _Value, _Value _defValue[]> _Value StateManager<_State, _Value, _defValue>::Get(_State state) const
{
	_States::const_iterator iter = _states.find(state);
	if (iter != _states.end())
		return iter->second;
	else
		return _defValue[state];
}

template<class _State, class _Value, _Value _defValue[]> void StateManager<_State, _Value, _defValue>::Set(_State state, _Value value)
{
	if (value != _defValue[state])
		_states[state] = value;
	else
	{
		_States::iterator iter = _states.find(state);
		if (iter != _states.end())
			_states.erase(iter);
	}
}

template<class _State, class _Value, _Value _defValue[]> void StateManager<_State, _Value, _defValue>::Restore(_State state)
{
	_states.erase(state);
}

template<class _State, class _Value, _Value _defValue[]> void StateManager<_State, _Value, _defValue>::Reset()
{
	_states.clear();
}

template<class _State, class _Value, _Value _defValue[]> typename StateManager<_State, _Value, _defValue>::iterator StateManager<_State, _Value, _defValue>::begin()
{
	return _states.begin();
}

template<class _State, class _Value, _Value _defValue[]> typename StateManager<_State, _Value, _defValue>::iterator StateManager<_State, _Value, _defValue>::end()
{
	return _states.end();
}

}

}

#endif