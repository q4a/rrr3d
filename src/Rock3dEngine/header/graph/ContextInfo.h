#ifndef R3D_GRAPH_CONTEXTINFO
#define R3D_GRAPH_CONTEXTINFO

#include "VideoResource.h"
#include "driver/RenderDriver.h"
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
		typename States::iterator iter = _states.find(state);
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
		typename States::iterator iter = _states.find(state);

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
		typename States::const_iterator iter = _states.find(state);

		LSL_ASSERT(iter != _states.end());

		return iter->second.top().value;
	}

	const _Value* End(const _State& state) const
	{
		typename States::const_iterator iter = _states.find(state);

		if (iter == _states.end())
			return 0;

		return iter->second.empty() ? 0 : &(iter->second.top().value);
	}
};

enum CombTransformStateType {ctsWorldView, ctsViewProj, ctsWVP, COMB_TRANSFORM_STATE_TYPE};

//��������� ����������� � �������, � ����� ������������ ������� ������ �������� � ������� �����������
class Renderable: public virtual lsl::Object
{
public:
	virtual void Render(Engine& engine) = 0;
};

//��������, ����������� ������
class RenderBuffer: public virtual lsl::Object
{
public:
	virtual void Render(Engine& engine, IDirect3DSurface9* backBuffer, IDirect3DSurface9* dsSurface) = 0;
};

struct MaterialDesc
{
	glm::vec4 diffuse;
	glm::vec4 ambient;
	glm::vec4 specular;
	glm::vec4 emissive;
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
	//������ ��������� �� ������� ������������ ����������� ��� csOrtho, csViewPort ������
	float width;

	glm::vec3 pos;
	glm::vec3 dir;
	glm::vec3 up;
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
		phi = glm::half_pi<float>();
		theta = glm::quarter_pi<float>();

		pos = NullVector;
		dir = XVector;
		up = ZVector;
		shadowMap = 0;
	}

	D3DLIGHTTYPE type;
	glm::vec4 ambient;
	glm::vec4 diffuse;
	glm::vec4 specular;

	//����������� ������ �������� ���������� ����������� ���������, ������������� ��������� �����. ��� ��������� ������������.
	float aspect;
	float nearDist;
	float range;
	float falloff;
	float attenuation0;
	float attenuation1;
	float attenuation2;
	float theta;
	float phi;

	glm::vec3 pos;
	glm::vec3 dir;
	glm::vec3 up;

	//����� ����� � ������������ ������� ������
	Tex2DResource* shadowMap;
};

class ContextInfo;

class CameraCI: public virtual lsl::Object
{
	friend ContextInfo;
public:
	enum Transform {ctView = 0, ctProj, ctWorldView, ctViewProj, ctWVP, cTransformEnd};

	static glm::vec2 ViewToProj(const glm::vec2& coord, const glm::vec2& viewSize);
	static glm::vec2 ProjToView(const glm::vec2& coord, const glm::vec2& viewSize);
private:
	CameraDesc _desc;

	D3DMATRIX _worldMat;
	mutable D3DMATRIX _matrices[cTransformEnd];
	mutable D3DMATRIX _invMatrices[cTransformEnd];
	mutable std::bitset<cTransformEnd> _matChanged;
	mutable std::bitset<cTransformEnd> _invMatChanged;
	//
	mutable Frustum _frustum;
	mutable bool _frustChanged;

	unsigned _idState;

	void CalcProjPerspective(D3DMATRIX& mat) const;

	void StateChanged();
	void WorldMatChanged(const D3DMATRIX& worldMat);
	void ProjMatChanged();
	void DescChanged();
public:
	CameraCI();

	//���������� ������������� ���������
	unsigned IdState() const;

	bool ComputeZBounds(const AABB& aabb, float& minZ, float& maxZ) const;
	void AdjustNearFarPlane(const AABB& aabb, float minNear, float maxFar);

	void GetProjPerspective(D3DMATRIX& mat) const;
	void GetViewProjPerspective(D3DMATRIX& mat) const;
	void GetWVPPerspective(D3DMATRIX& mat) const;
	void SetProjMat(const D3DMATRIX& value);

	glm::vec3 ScreenToWorld(const glm::vec2& coord, float z, const glm::vec2& viewSize) const;
	glm::vec2 WorldToScreen(const glm::vec3& coord, const glm::vec2& viewSize) const;

	const CameraDesc& GetDesc() const;
	void SetDesc(const CameraDesc& value);

	const D3DMATRIX& GetTransform(Transform transform) const;
	const D3DMATRIX& GetInvTransform(Transform transform) const;
	const Frustum& GetFrustum() const;

	const D3DMATRIX& GetView() const;
	const D3DMATRIX& GetProj() const;
	const D3DMATRIX& GetViewProj() const;
	const D3DMATRIX& GetWVP() const;
	//
	const D3DMATRIX& GetInvView() const;
	const D3DMATRIX& GetInvProj() const;
	const D3DMATRIX& GetInvViewProj() const;
	const D3DMATRIX& GetInvWVP() const;
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
	//������ �������� � ������� �������� �������
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

	static const TransformStateType cTexTransform[8];
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

	D3DMATRIX _worldMat;

	std::vector<D3DMATRIX> _textureMatStack[cMaxTexSamplers];
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
	glm::vec4 _color;
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

	//������������ ��������� �������� �������� �������, ������� ���� �������� ������ �� ���, ����� ������ ������ �� ������������� �������� ��� �������
	//���������� true ���� ������ ������ ����� ������������
	void PushShader(BaseShader* value);
	void PopShader(BaseShader* value);
	bool IsShaderActive() const;

	const D3DMATRIX& GetWorldMat() const;
	void SetWorldMat(const D3DMATRIX& value);

	void PushTextureTransform(int stage, const D3DMATRIX& value);
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

	//������� �������� �������� � ���������� [0..1](������...�����), ����� �� ���� ���������� �������� � ��������� ������������ ����� ����� ����� (������ ������������)
	float GetFrame() const;
	void PushFrame(float value);
	void PopFrame();

	//������� ������ ��� ���������� ������������ ��� ���������
	float GetCullOpacity() const;
	void SetCullOpacity(float value);
	void RestoreCullOpacity();
	bool IsCullOpacity() const;

	const glm::vec4& GetColor() const;
	void SetColor(const glm::vec4& value);

	int GetMeshId() const;
	void SetMeshId(int value);

	bool IsNight() const;
};

template<class _State, class _Value, _Value _defValue[]> _Value StateManager<_State, _Value, _defValue>::Get(_State state) const
{
	typename _States::const_iterator iter = _states.find(state);
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
		typename _States::iterator iter = _states.find(state);
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