#ifndef R3D_GRAPH_VIDEORESOURCE
#define R3D_GRAPH_VIDEORESOURCE

#include "res\\GraphResource.h"

namespace r3d
{

namespace graph
{

class Engine;

class IVideoResource
{
public:
	virtual void OnLostDevice() {}
	virtual void OnResetDevice() {}
};

class VideoResource: public lsl::Resource, public IVideoResource
{
private:
	typedef lsl::Resource _MyBase;
private:
	Engine* _engine;

	bool _autoInit;

	void SetEngine(Engine* value);
public:
	VideoResource();
	virtual ~VideoResource();

	void Init(Engine& engine);
	void Free();
	void Reload(Engine& engine);
	void Reload();

	virtual void OnLostDevice();
	virtual void OnResetDevice();

	Engine* GetEngine();
};

class MemPoolResource: public VideoResource
{
private:	
	D3DPOOL _memoryPool;
	DWORD _usage;
public:
	MemPoolResource(DWORD usage);

	virtual void OnLostDevice();
	virtual void OnResetDevice();

	D3DPOOL GetMemoryPool() const;
	void SetMemoryPool(D3DPOOL value);
	DWORD GetUsage() const;
	void SetUsage(DWORD value);	
};

class VBMesh: public MemPoolResource
{
private:
	res::VertexData* _data;
	bool _createData;
	unsigned _beginStream;
	
	IDirect3DVertexBuffer9* _vb;
protected:
	void LoadData() const;

	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	VBMesh();
	virtual ~VBMesh();

	void BeginStream(unsigned offsetInBytes, unsigned stride, unsigned streamNumber = 0);
	void BeginStream();
	void EndStream();

	void Draw(unsigned startVertex, unsigned primitiveCount);
	void Draw();

	res::VertexData* GetData();
	res::VertexData* GetOrCreateData();
	void SetData(res::VertexData* value);

	const D3DXVECTOR3& GetMinPos() const;
	const D3DXVECTOR3& GetMaxPos() const;
	IDirect3DVertexBuffer9* GetVB() const;

	D3DPRIMITIVETYPE primitiveType;
};

class IndexedVBMesh: public MemPoolResource
{
private:
	res::MeshData* _data;
	bool _createData;
	unsigned _beginStreamCnt;

	IDirect3DVertexBuffer9* _vb;
	IDirect3DIndexBuffer9* _ib;	
protected:
	void LoadData() const;

	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:	
	IndexedVBMesh();
	virtual ~IndexedVBMesh();

	void BeginStream(int attribId, unsigned streamNumber = 0);
	void BeginStream();
	void EndStream();

	void Draw();
	void DrawSubset(int attribId);

	res::MeshData* GetData();
	res::MeshData* GetOrCreateData();
	void SetData(res::MeshData* value);

	unsigned GetSubsetCount() const;
	const D3DXVECTOR3& GetMinPos() const;
	const D3DXVECTOR3& GetMaxPos() const;
	
	IDirect3DVertexBuffer9* GetVB();
	IDirect3DIndexBuffer9* GetIB();
};

class MeshX: public MemPoolResource
{
public:
	enum Prefab {mpSphere = 0, cPrefabEnd};

	struct Params
	{
		float radius1;
		int slices;
		int stacks;

		Params(): radius1(1.0f), slices(15), stacks(15) {}
	};
private:
	res::MeshData* _data;
	bool _createData;	
	Prefab _prefab;
	Params _params;

	ID3DXMesh* _d3dxMesh;
	unsigned _beginStreamCnt;
protected:
	void LoadData() const;

	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:	
	MeshX();
	virtual ~MeshX();

	void BeginStream(int attribId, unsigned streamNumber = 0);
	void BeginStream();
	void EndStream();

	void Draw();
	void DrawSubset(int attribId);

	res::MeshData* GetData();
	res::MeshData* GetOrCreateData();
	void SetData(res::MeshData* value);

	Prefab GetPrefab() const;
	void SetPrefab(Prefab value);

	unsigned GetSubsetCount() const;
	const D3DXVECTOR3& GetMinPos() const;
	const D3DXVECTOR3& GetMaxPos() const;
};

class TexResource: public MemPoolResource
{
private:
	unsigned _levelCnt;
	D3DXVECTOR2 _screenScale;
protected:
	virtual IDirect3DBaseTexture9* GetBaseTex() = 0;
public:
	TexResource(DWORD usage);

	IDirect3DBaseTexture9* GetTex();
	
	unsigned GetLevelCnt() const;
	//value = 0 - генерация последовательности mip уровней до 1x1
	void SetLevelCnt(unsigned value);

	const D3DXVECTOR2& GetScreenScale() const;
	void SetScreenScale(D3DXVECTOR2 value);
};

class Tex2DResource: public TexResource
{
private:
	res::ImageResource* _data;
	bool _createData;
	bool _d3dxLoadUsed;
	bool _gui;

	IDirect3DTexture9* _texture;

	virtual IDirect3DBaseTexture9* GetBaseTex();
	void LoadData();
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	Tex2DResource();
	virtual ~Tex2DResource();

	void SyncFrom(Tex2DResource* value);

	bool GetGUI() const;
	void SetGUI(bool value);

	res::ImageResource* GetData();
	const res::ImageResource* GetData() const;

	res::ImageResource* GetOrCreateData();
	void SetData(res::ImageResource* value);

	IDirect3DTexture9* GetTex();
	D3DXVECTOR2 GetSize();
};

class TexCubeResource: public TexResource
{
private:
	res::CubeImageResource* _data;
	bool _createData;
	bool _d3dxLoadUsed;

	IDirect3DCubeTexture9* _texture;

	virtual IDirect3DBaseTexture9* GetBaseTex();
	void LoadData();
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	TexCubeResource();
	virtual ~TexCubeResource();

	res::CubeImageResource* GetData();
	const res::CubeImageResource* GetData() const;
	res::CubeImageResource* GetOrCreateData();
	void SetData(res::CubeImageResource* value);

	IDirect3DCubeTexture9* GetTex();	
};

class RenderTargetResource: public VideoResource
{
private:
	IDirect3DSurface9* _surface;

	unsigned _width;
	unsigned _height;
	D3DFORMAT _format;
	bool _lockable;
	D3DMULTISAMPLE_TYPE _multisampleType;
	unsigned _multisampleQuality;

	D3DXVECTOR2 _screenScale;
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	RenderTargetResource();
	virtual ~RenderTargetResource();

	IDirect3DSurface9* GetSurface();

	unsigned GetWidth() const;
	void SetWidth(unsigned value);

	unsigned GetHeight() const;
	void SetHeight(unsigned value);

	D3DFORMAT GetFormat() const;
	void SetFormat(D3DFORMAT value);

	bool GetLockable() const;
	void SetLockable(bool value);	

	D3DMULTISAMPLE_TYPE GetMultisampleType() const;
	void SetMultisampleType(D3DMULTISAMPLE_TYPE value);

	unsigned GetMultisampleQuality() const;
	void SetMultisampleQuality(unsigned value);

	const D3DXVECTOR2& GetScreenScale() const;
	void SetScreenScale(D3DXVECTOR2 value);
};

class DepthStencilSurfaceResource: public VideoResource
{
private:
	IDirect3DSurface9* _surface;

	unsigned _width;
	unsigned _height;
	D3DFORMAT _format;
	bool _discard;
	D3DMULTISAMPLE_TYPE _multisampleType;
	unsigned _multisampleQuality;

	D3DXVECTOR2 _screenScale;
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	DepthStencilSurfaceResource();
	virtual ~DepthStencilSurfaceResource();

	IDirect3DSurface9* GetSurface();

	unsigned GetWidth() const;
	void SetWidth(unsigned value);

	unsigned GetHeight() const;
	void SetHeight(unsigned value);

	D3DFORMAT GetFormat() const;
	void SetFormat(D3DFORMAT value);

	bool GetDiscard() const;
	void SetDiscard(bool value);	

	D3DMULTISAMPLE_TYPE GetMultisampleType() const;
	void SetMultisampleType(D3DMULTISAMPLE_TYPE value);

	unsigned GetMultisampleQuality() const;
	void SetMultisampleQuality(unsigned value);

	const D3DXVECTOR2& GetScreenScale() const;
	void SetScreenScale(D3DXVECTOR2 value);
};

class TextFont: public VideoResource
{
private:
	ID3DXFont* _font;
public:
	struct Desc
	{
		Desc()
		{
			height = 14;
			width = 0;
			weight = FW_BOLD;
			mipLevels = 0;
			italic = false;
			charSet = DEFAULT_CHARSET;
			outputPrecision = OUT_DEFAULT_PRECIS;
			quality = DEFAULT_QUALITY;
			pitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
			pFacename = "Arial";
		}

		int height;
		unsigned width;
		unsigned weight;
		unsigned mipLevels;
		bool italic;
		DWORD charSet;
		DWORD outputPrecision;
		DWORD quality;
		DWORD pitchAndFamily;
		std::string pFacename;
	};
private:
	Desc _desc;
protected:
	virtual void DoInit();
	virtual void DoFree();
	virtual void DoUpdate();
public:
	TextFont();
	virtual ~TextFont();

	void DrawText(const std::string& text, lsl::Rect& rect, DWORD format, const D3DCOLOR& color);
	void DrawText(const std::wstring& text, lsl::Rect& rect, DWORD format, const D3DCOLOR& color);

	virtual void OnLostDevice();
	virtual void OnResetDevice();

	const Desc& GetDesc() const;
	void SetDesc(const Desc& value);
};

}

}

#endif