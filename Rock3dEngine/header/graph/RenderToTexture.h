#ifndef RENDER_TO_TEXTURE
#define RENDER_TO_TEXTURE

#include "Engine.h"

namespace r3d
{

namespace graph
{

template<class _TexRes> class RenderTarget: public lsl::Object
{
};

template<> class RenderTarget<Tex2DResource>: public lsl::Object
{
public:
	struct RtFlags
	{
		RtFlags(DWORD mSurfLevel, DWORD mClearFlags, D3DCOLOR mColor = 0, float mZ = 1.0f, DWORD mStencil = 0): surfLevel(mSurfLevel), clearFlags(mClearFlags), color(mColor), z(mZ), stencil(mStencil) {};

		DWORD surfLevel;
		DWORD clearFlags;
		D3DCOLOR color;
		float z;
		DWORD stencil;
	};
private:
	Tex2DResource* _texture;
	bool _createTexture;
	IDirect3DSurface9* _surfRT;
protected:
	virtual Tex2DResource* CreateRT();
	
	void ApplyRT(Engine& engine, const RtFlags& flags);
	void UnApplyRT(Engine& engine);
	bool IsApplyRT() const;
public:
	RenderTarget();
	virtual ~RenderTarget();

	Tex2DResource* GetRT();
	Tex2DResource* GetOrCreateRT();
	void SetRT(Tex2DResource* value);
};

template<> class RenderTarget<TexCubeResource>: public lsl::Object
{
public:
	struct RtFlags
	{
		RtFlags(D3DCUBEMAP_FACES mFaceType, DWORD mSurfLevel, DWORD mClearFlags, D3DCOLOR mColor = 0, float mZ = 1.0f, DWORD mStencil = 0): faceType(mFaceType), surfLevel(mSurfLevel), clearFlags(mClearFlags), color(mColor), z(mZ), stencil(mStencil) {};

		D3DCUBEMAP_FACES faceType;
		DWORD surfLevel;
		DWORD clearFlags;
		D3DCOLOR color;
		float z;
		DWORD stencil;
	};
private:
	TexCubeResource* _texture;
	bool _createTexture;
	IDirect3DSurface9* _surfRT;
protected:
	virtual TexCubeResource* CreateRT();
	
	void ApplyRT(Engine& engine, const RtFlags& flags);
	void UnApplyRT(Engine& engine);
	bool IsApplyRT() const;
public:
	RenderTarget();
	virtual ~RenderTarget();

	TexCubeResource* GetRT();
	TexCubeResource* GetOrCreateRT();
	void SetRT(TexCubeResource* value);
};

template<class _TexRes> class GraphObjRender: public RenderTarget<_TexRes>
{
private:
	typedef RenderTarget<_TexRes> _MyBase;
public:
	typedef typename _MyBase::RtFlags RtFlags;
private:
	bool _isBeginRT;
public:
	GraphObjRender(): _isBeginRT(false) {};

	virtual void BeginRT(Engine& engine, const RtFlags& flags)
	{
		LSL_ASSERT(!_isBeginRT);
		
		_isBeginRT = true;	
	}

	virtual void EndRT(Engine& engine)
	{
		LSL_ASSERT(_isBeginRT);
		
		_isBeginRT = false;	
	}

	bool IsBeginRT() const
	{
		return _isBeginRT;
	}
};

template<class _TexRes> class PostEffRender: public RenderTarget<_TexRes>
{
public:
	virtual void Render(Engine& engine) = 0;
};

class RenderToTexture: public GraphObjRender<Tex2DResource>
{
private:
	typedef GraphObjRender<Tex2DResource> _MyBase;
public:
	virtual void BeginRT(Engine& engine, const RtFlags& flags);
	virtual void EndRT(Engine& engine);
};

class RenderToCubeTex: public GraphObjRender<TexCubeResource>
{
private:
	typedef GraphObjRender<TexCubeResource> _MyBase;

	CameraCI _myCamera;
private:
	RtFlags _flags;
	D3DXVECTOR3 _viewPos;
public:
	RenderToCubeTex();

	void BeginCubeSurf(Engine& engine);
	bool EndCubeSurf(Engine& engine, bool nextPass);

	virtual void BeginRT(Engine& engine, const RtFlags& flags);
	virtual void EndRT(Engine& engine);

	const D3DXVECTOR3& GetViewPos() const;
	void SetViewPos(const D3DXVECTOR3& value);
};

}

}

#endif