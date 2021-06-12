#ifndef R3D_GRAPH_ACTOR
#define R3D_GRAPH_ACTOR

#include "include\\graph\\IActor.h"

#include "SceneNode.h"

namespace r3d
{

class GraphManager;

namespace graph
{

class Actor: public IActor, public SceneNode
{
	friend GraphManager;
	friend class ActorManager;
private:
	typedef SceneNode _MyBase;	
private:
	GraphManager* _graph;
	GraphDesc _graphDesc;
	D3DXVECTOR4 _vec1;
	D3DXVECTOR4 _vec2;
	D3DXVECTOR4 _vec3;
	float _texDiffK;

	lsl::Object* _user;
protected:
	virtual void DoRender(graph::Engine& engine);

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
	virtual void OnFixUp(const FixUpNames& fixUpNames);
public:
	Actor();
	virtual ~Actor();

	void ReleaseGraph();

	GraphManager* GetGraph();

	const GraphDesc& GetGraphDesc() const;
	void SetGraph(GraphManager* graph, const GraphDesc& desc);	

	//planar refl plane(norm(x, y, z), d(w))
	//if not null then it is a track
	const D3DXVECTOR4& vec1() const;
	void vec1(const D3DXVECTOR4& value);

	//planar refl border back(x), front(y), left(z), right(w)
	const D3DXVECTOR4& vec2() const;
	void vec2(const D3DXVECTOR4& value);

	//planar refl meshId(x), minTexDiffK(y), maxTexDiffK(z)
	//refl bumb (x)
	const D3DXVECTOR4& vec3() const;
	void vec3(const D3DXVECTOR4& value);

	float texDiffK() const;
	void texDiffK(float value);
};

}

}

#endif