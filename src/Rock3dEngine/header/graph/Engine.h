#ifndef R3D_GRAPH_ENGINE
#define R3D_GRAPH_ENGINE

#include "ContextInfo.h"
#include "VideoResource.h"

namespace r3d
{

namespace graph
{

class Engine: public lsl::Object
{
private:
	typedef std::list<IVideoResource*> _VideoResList;

	static bool _d3dxUse;
private:
	RenderDriver* _driver;
	ContextInfo* _context;

	D3DPRESENT_PARAMETERS _d3dpp;
	IDirect3DQuery9* _d3dQueryEvent;
	IDirect3DQuery9* _d3dQueryBuf[2];
	IDirect3DSwapChain9* _swapChain;
	IDirect3DSurface9* _dsSurf;
	IDirect3DSurface9* _backBuff;
	_VideoResList _videoResList;	

	//Текущее состояние устройства
	volatile bool _reset;
	//Указывает на то что устройство было потеряно и необходимо перейти в состояние _reset = false
	volatile bool _lost;
	//
	volatile bool _restart;

	bool _beginScene;
	bool _beginDraw;
	float _dt;
	bool _pause;
	unsigned _filtering;
	unsigned _multisampling;

	graph::VBMesh _meshPlanePT;
	graph::VBMesh _meshScreenQuad;

	void CreateQueries();
	void ReleaseQueries();
	void UpdateScreenQuad();

	void InitResources();
	void FreeResources();

	bool ResetDevice();
	void LostDevice();
	void SetParams(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling);

#ifdef _DEBUG
	ID3DXFont* g_pd3dxFont;
	void DrawFPS();
#endif
public:
	Engine(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling);
	~Engine();

	void InsertVideoRes(IVideoResource* value);
	void RemoveVideoRes(IVideoResource* value);

	//Проверка устройства и его сброс. Выполняются в главном потоке.
	bool CheckReset();
	//Сброс устройства с новыми параметрами
	bool Reset(HWND window, lsl::Point resolution, bool fullScreen, unsigned multisampling, bool resetDevice);
	bool IsReset() const;
	//Рестарт движка, иначе говоря первый запуск или повторный с потерянной свзяью с предыдущим
	bool IsRestart() const;

	void Restart();
	void ProgressTime(float dt, bool pause);

	//true если сцену можно рендерить
	bool BeginScene();
	//завершение сцены, обязательно после BeginScene
	bool EndScene();
	//
	void GPUSync();
	bool IsSyncSupported();
	//
	bool Present();

	//Связка методов позволяет контролировать начало и конец отрисовки цельного примитва с одним набором трансформаций и материалов. Контроль нужен чтобы осуществлять менеджмент шейдеров. Позволяет отрисовывать целые узлы сцены с использованием строго определенного шейдера
	void BeginDraw();
	//Возвращает false если нужен ещё один проход, при этом BeginDraw повторно не вызывается. Если ожидается(или возможен, разрешен) следующий проход то в арг. nextPass следует передывать true
	bool EndDraw(bool nextPass);

	void BeginBackBufOut(DWORD clearFlags, D3DCOLOR color);
	void EndBackBufOut();
	IDirect3DSurface9* GetDSSurf();

	void BeginMeshPT();	
	void EndMeshPT();
	void RenderPlanePT();
	void RenderSpritePT(const D3DXVECTOR3& pos, const D3DXVECTOR3& scale, float turnAngle, const D3DXVECTOR3* fixDirection, const D3DXMATRIX& localMat);

	void RenderScreenQuad(bool disableZBuf = false);

	float GetDt(bool ignorePause = false);

	unsigned GetFiltering() const;
	void SetFiltering(unsigned value);

	unsigned GetMultisampling() const;

	static bool d3dxUse();
	static void d3dxUse(bool value);
	
	RenderDriver& GetDriver() const;
	const D3DPRESENT_PARAMETERS& GetParams() const;

	ContextInfo& GetContext();
	const ContextInfo& GetContext() const;

	static void ToMultisampling(unsigned level, D3DMULTISAMPLE_TYPE& type, unsigned& quality);
};

class RenderStateManager: public StateManager<RenderState, DWORD, ContextInfo::defaultRenderStates>
{
public:
	void Apply(Engine& engine);
	void UnApply(Engine& engine);
};

class SamplerStateManager: public StateManager<SamplerState, DWORD, ContextInfo::defaultSamplerStates>
{
public:
	void Apply(Engine& engine, DWORD stage);
	void UnApply(Engine& engine, DWORD stage);
};

//: public StateManager<TextureStageState, DWORD, ContextInfo::defaultTextureStageStates>
class TextureStageStateManager
{
private:
	typedef std::map<TextureStageState, DWORD> _States;
public:
	typedef _States::iterator iterator;	
private:
	_States _states;
public:
	DWORD Get(TextureStageState state) const;
	void Set(TextureStageState state, DWORD value);
	void Restore(TextureStageState state);
	void Reset();

	iterator begin();
	iterator end();

	void Apply(Engine& engine, DWORD stage);
	void UnApply(Engine& engine, DWORD stage);
};

}

}

#endif