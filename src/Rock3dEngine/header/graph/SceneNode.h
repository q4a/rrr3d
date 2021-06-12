#ifndef R3D_GRAPH_SCENENODE
#define R3D_GRAPH_SCENENODE

#include "FxManager.h"
#include "WaterPlane.h"
#include "GrassField.h"
#include "StdNode.h"

namespace r3d
{

namespace graph
{

class SceneNode: public BaseSceneNode
{
private:
	typedef BaseSceneNode _MyBase;
public:
	enum NodeType{ntNode = 0, ntIVBMesh, ntPlane, ntSprite, ntScreenSprite, ntWaterPlane, ntGrassField, ntParticleSystem, ntMovCoordSys, ntScaleCoordSys, cNodeTypeEnd};
	static const char* cNodeTypeStr[cNodeTypeEnd];

	typedef lsl::ClassList<NodeType, BaseSceneNode> ClassList;
	static ClassList classList;

	class Nodes: public lsl::Collection<BaseSceneNode, NodeType, void, void>
	{
	private:
		typedef lsl::Collection<BaseSceneNode, NodeType, void, void> _MyBase;
	private:
		SceneNode* _owner;
	protected:
		virtual void InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			value->SetOwner(_owner);
			value->SetParent(_owner);
		}
	public:
		Nodes(SceneNode* owner): _owner(owner) {}
	};

	class Proxy: public BaseSceneNode
	{	
	private:
		SceneNode* _owner;
	protected:
		virtual void DoRender(graph::Engine& engine)
		{
			_owner->RenderProxy(engine, this);
		}		
	public:
		Proxy() {}
		Proxy(SceneNode* owner): _owner(owner) {}
	};

	class ProxyList: public lsl::Collection<Proxy, void, void, void>
	{
	private:
		typedef lsl::Collection<Proxy, void, void, void> _MyBase;
	private:
		SceneNode* _owner;
	protected:
		virtual void InsertItem(const Value& value)
		{
			_MyBase::InsertItem(value);

			value->SetOwner(_owner);			
		}
	public:
		ProxyList(SceneNode* owner): _owner(owner) {}

		Proxy& Add()
		{
			return _MyBase::AddItem(new Proxy(_owner));
		}
	};

	static void InitClassList();	
private:
	Nodes* _nodes;
	ProxyList* _proxyList;
protected:
	virtual void ChildStructureChanged(BaseSceneNode* child);

	virtual void DoRender(graph::Engine& engine);
	virtual void RenderProxy(graph::Engine& engine, Proxy* proxy);
	virtual AABB LocalDimensions() const;

	virtual void Save(lsl::SWriter* writer);
	virtual void Load(lsl::SReader* reader);
public:
	SceneNode();
	virtual ~SceneNode();

	Nodes& GetNodes();
	ProxyList& GetProxyList();

	BaseSceneNode* GetNodeByTag(int tag);
};

}

}

#endif