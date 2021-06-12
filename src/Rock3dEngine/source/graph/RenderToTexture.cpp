#include "stdafx.h"

#include "graph\\RenderToTexture.h"

namespace r3d
{

namespace graph
{

namespace
{

D3DCUBEMAP_FACES operator++(D3DCUBEMAP_FACES& value)
{
	return value = static_cast<D3DCUBEMAP_FACES>(value + 1);
}

}




RenderTarget<Tex2DResource>::RenderTarget(): _texture(0), _createTexture(false)
{
}

RenderTarget<Tex2DResource>::~RenderTarget()
{
	SetRT(0);
}

Tex2DResource* RenderTarget<Tex2DResource>::CreateRT()
{
	Tex2DResource* tex = new Tex2DResource();
	tex->SetDynamic(true);
	tex->SetUsage(D3DUSAGE_RENDERTARGET);
	
	return tex;
}

void RenderTarget<Tex2DResource>::ApplyRT(Engine& engine, const RtFlags& flags)
{
	GetRT()->GetTex()->GetSurfaceLevel(flags.surfLevel, &_surfRT);
	HRESULT hr = engine.GetDriver().GetDevice()->SetRenderTarget(0, _surfRT);

	LSL_ASSERT(hr == D3D_OK);

	if (flags.clearFlags > 0)
		engine.GetDriver().GetDevice()->Clear(0, 0, flags.clearFlags, flags.color, flags.z, flags.stencil);
}

void RenderTarget<Tex2DResource>::UnApplyRT(Engine& engine)
{
	lsl::SafeRelease(_surfRT);
}

bool RenderTarget<Tex2DResource>::IsApplyRT() const
{
	return _surfRT != 0;
}

Tex2DResource* RenderTarget<Tex2DResource>::GetRT()
{
	return _texture;	
}

Tex2DResource* RenderTarget<Tex2DResource>::GetOrCreateRT()
{
	if (!_texture)
	{
		_texture = CreateRT();
		_texture->AddRef();
		_createTexture = true;
	}
	return _texture;
}

void RenderTarget<Tex2DResource>::SetRT(Tex2DResource* value)
{
	if (ReplaceRef(_texture, value))
	{
		if (_createTexture)
		{
			delete _texture;
			_createTexture = false;
		}
		_texture = value;
	}
}




RenderTarget<TexCubeResource>::RenderTarget(): _texture(0), _createTexture(false)
{
}

RenderTarget<TexCubeResource>::~RenderTarget()
{
	SetRT(0);
}

TexCubeResource* RenderTarget<TexCubeResource>::CreateRT()
{
	TexCubeResource* tex = new TexCubeResource();
	tex->SetDynamic(true);
	tex->SetUsage(D3DUSAGE_RENDERTARGET);

	return tex;
}

void RenderTarget<TexCubeResource>::ApplyRT(Engine& engine, const RtFlags& flags)
{
	GetRT()->GetTex()->GetCubeMapSurface(flags.faceType, flags.surfLevel, &_surfRT);
	HRESULT hr = engine.GetDriver().GetDevice()->SetRenderTarget(0, _surfRT);

	LSL_ASSERT(hr == D3D_OK);

	if (flags.clearFlags > 0)
		engine.GetDriver().GetDevice()->Clear(0, 0, flags.clearFlags, flags.color, flags.z, flags.stencil);
}

void RenderTarget<TexCubeResource>::UnApplyRT(Engine& engine)
{
	lsl::SafeRelease(_surfRT);
}

bool RenderTarget<TexCubeResource>::IsApplyRT() const
{
	return _surfRT != 0;
}

TexCubeResource* RenderTarget<TexCubeResource>::GetRT()
{
	return _texture;	
}

TexCubeResource* RenderTarget<TexCubeResource>::GetOrCreateRT()
{
	if (!_texture)
	{
		_texture = CreateRT();
		_texture->AddRef();
		_createTexture= true;
	}
	return _texture;
}

void RenderTarget<TexCubeResource>::SetRT(TexCubeResource* value)
{
	if (ReplaceRef(_texture, value))
	{
		if (_createTexture)
		{
			delete _texture;
			_createTexture = false;
		}
		_texture = value;
	}
}




void RenderToTexture::BeginRT(Engine& engine, const RtFlags& flags)
{
	ApplyRT(engine, flags);

	_MyBase::BeginRT(engine, flags);
}

void RenderToTexture::EndRT(Engine& engine)
{
	UnApplyRT(engine);

	_MyBase::EndRT(engine);
}




RenderToCubeTex::RenderToCubeTex(): _flags(static_cast<D3DCUBEMAP_FACES>(0), 0, 0), _viewPos(NullVector)
{
}

void RenderToCubeTex::BeginCubeSurf(Engine& engine)
{
	LSL_ASSERT(IsBeginRT());

	CameraDesc camDesc = engine.GetContext().GetCamera().GetDesc();
	//camDesc.pos = D3DXVECTOR3(0, 0, 15.0f);
	camDesc.pos = _viewPos;
	camDesc.style = csPerspective;
	camDesc.aspect = 1;
	camDesc.nearDist = 1.0f;
	camDesc.farDist = 100.0f;
	camDesc.fov = D3DX_PI/2;

	switch(_flags.faceType)
	{
	case D3DCUBEMAP_FACE_POSITIVE_X:
		camDesc.dir = D3DXVECTOR3(-1.0f, 0.0f, 0.0f );
		camDesc.up = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
		break;

	case D3DCUBEMAP_FACE_NEGATIVE_X:			
		camDesc.dir = D3DXVECTOR3(1.0f, 0.0f, 0.0f );			
		camDesc.up = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
		break; 

	case D3DCUBEMAP_FACE_POSITIVE_Y:			
		camDesc.dir = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
		camDesc.up = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
		break;
			
	case D3DCUBEMAP_FACE_NEGATIVE_Y:			
		camDesc.dir = D3DXVECTOR3( 0.0f, 1.0f, 0.0f );
		camDesc.up = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
		break;				
			
	case D3DCUBEMAP_FACE_POSITIVE_Z:
		camDesc.dir = D3DXVECTOR3( 0.0f, 0.0f, -1.0f );
		camDesc.up = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
		break;				
	
	case D3DCUBEMAP_FACE_NEGATIVE_Z:
		camDesc.dir = D3DXVECTOR3( 0.0f, 0.0f, 1.0f );
		camDesc.up = D3DXVECTOR3( 0.0f, -1.0f, 0.0f );
		break;
	}

	_myCamera.SetDesc(camDesc);	
	engine.GetContext().ApplyCamera(&_myCamera);

	ApplyRT(engine, _flags);
}

bool RenderToCubeTex::EndCubeSurf(Engine& engine, bool nextPass)
{
	UnApplyRT(engine);

	engine.GetContext().UnApplyCamera(&_myCamera);

	if (nextPass && ++_flags.faceType < 6)
	{
		BeginCubeSurf(engine);
		return false;
	}
	else
	{
		_flags.faceType = static_cast<D3DCUBEMAP_FACES>(0);
		return true;
	}
}

void RenderToCubeTex::BeginRT(Engine& engine, const RtFlags& flags)
{
	_flags = flags;
	_flags.faceType = static_cast<D3DCUBEMAP_FACES>(0);

	_MyBase::BeginRT(engine, flags);
}

void RenderToCubeTex::EndRT(Engine& engine)
{
	_MyBase::EndRT(engine);
}

const D3DXVECTOR3& RenderToCubeTex::GetViewPos() const
{
	return _viewPos;
}

void RenderToCubeTex::SetViewPos(const D3DXVECTOR3& value)
{
	_viewPos = value;
}

}

}