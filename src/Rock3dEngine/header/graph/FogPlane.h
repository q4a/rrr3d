#ifndef R3D_GRAPH_FOGPLANE
#define R3D_GRAPH_FOGPLANE

#include "StdNode.h"

namespace r3d
{

namespace graph
{

class FogPlane: public BaseSceneNode
{
private:
	PlaneNode _plane;

	Tex2DResource* _depthTex;
	graph::LibMaterial * _cloudsMat;
	glm::vec4 _color;
	float _cloudIntens;
	float _speed;

	float _curTime;
protected:
	virtual void DoRender(graph::Engine& engine);
public:
	FogPlane();
	virtual ~FogPlane();

	Tex2DResource* GetDepthTex();
	void SetDepthTex(Tex2DResource* value);

	graph::LibMaterial* GetCloudsMat();
	void SetCloudsMat(graph::LibMaterial* value);

	const glm::vec4& GetColor() const;
	void SetColor(const glm::vec4& value);

	float GetCloudIntens() const;
	void SetCloudIntens(float value);

	float GetSpeed() const;
	void SetSpeed(float value);

	const glm::vec2& GetSize() const;
	void SetSize(const glm::vec2& value);

	Shader shader;
};

}

}

#endif