#ifndef R3D_SCENE_WATER_PLANE
#define R3D_SCENE_WATER_PLANE

#include "StdNode.h"
#include "graph/RenderToTexture.h"

namespace r3d
{

namespace graph
{

class ReflRender: public GraphObjRender<Tex2DResource>
{
private:
	typedef GraphObjRender<Tex2DResource> _MyBase;
public:
	typedef std::vector<glm::vec4> ClipPlanes;
private:
	glm::vec4 _reflPlane;
	ClipPlanes _clipPlanes;

	D3DMATRIX _reflMat;
	CameraCI _reflCamera;
public:
	ReflRender();

	virtual void BeginRT(Engine& engine, const RtFlags& flags);
	virtual void EndRT(Engine& engine);

	const glm::vec4& GetReflPlane() const;
	void SetReflPlane(const glm::vec4& value);

	const ClipPlanes& GetClipPlanes() const;
	void SetClipPlanes(const ClipPlanes& value);
};

class WaterPlane: public PlaneNode
{
private:
	typedef PlaneNode _MyBase;
private:
	glm::vec3 _viewPos;
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

	glm::vec4 GetColor();
	void SetColor(const glm::vec4& value);

	const glm::vec3& GetViewPos() const;
	void SetViewPos(const glm::vec3& value);

	float GetCloudIntens() const;
	void SetCloudIntens(float value);

	Shader shader;
};

}

}

#endif