#ifndef R3D_SCENE_WATER_PLANE
#define R3D_SCENE_WATER_PLANE

#include "StdNode.h"
#include "graph\\RenderToTexture.h"

namespace r3d
{

namespace graph
{

class ReflRender: public GraphObjRender<Tex2DResource>
{
private:
	typedef GraphObjRender<Tex2DResource> _MyBase;
public:
	typedef std::vector<D3DXPLANE> ClipPlanes;
private:
	D3DXPLANE _reflPlane;
	ClipPlanes _clipPlanes;

	D3DXMATRIX _reflMat;
	CameraCI _reflCamera;
public:
	ReflRender();

	virtual void BeginRT(Engine& engine, const RtFlags& flags);
	virtual void EndRT(Engine& engine);

	const D3DXPLANE& GetReflPlane() const;
	void SetReflPlane(const D3DXPLANE& value);

	const ClipPlanes& GetClipPlanes() const;
	void SetClipPlanes(const ClipPlanes& value);
};

class WaterPlane: public PlaneNode
{
private:
	typedef PlaneNode _MyBase;
private:
	D3DXVECTOR3 _viewPos;
	float _cloudIntens;
protected:
	virtual void DoRender(graph::Engine& engine);
public:
	WaterPlane();

	Tex2DResource* GetDepthTex();
	void SetDepthTex(Tex2DResource* value);

	graph::Tex2DResource* GetNormTex();
	void SetNormTex(graph::Tex2DResource* value);

	graph::Tex2DResource* GetReflTex();
	void SetReflTex(graph::Tex2DResource* value);

	D3DXCOLOR GetColor();
	void SetColor(const D3DXCOLOR& value);

	const D3DXVECTOR3& GetViewPos() const;
	void SetViewPos(const D3DXVECTOR3& value);

	float GetCloudIntens() const;
	void SetCloudIntens(float value);

	Shader shader;
};

}

}

#endif