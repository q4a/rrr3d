#include "stdafx.h"

#include "graph\\Actor.h"
#include "GraphManager.h"

namespace r3d
{

namespace graph
{

Actor::Actor(): _graph(0), _vec1(0, 0, 0, 0), _vec2(0, 0, 0, 0), _vec3(0, 0, 0, 0), _texDiffK(1.0f), _user(0)
{
}

Actor::~Actor()
{
	ReleaseGraph();

	LSL_ASSERT(!_user);
}

void Actor::DoRender(graph::Engine& engine)
{
	engine.GetContext().SetLightShadow(_graphDesc.props.test(gpShadowApp));
	engine.GetContext().SetTexDiffK(_texDiffK);
	
	if (_graphDesc.lighting == glNone)
		engine.GetContext().SetRenderState(graph::rsLighting, false);

	_MyBase::DoRender(engine);

	engine.GetContext().RestoreLightShadow();

	if (_graphDesc.lighting == glNone)
		engine.GetContext().RestoreRenderState(graph::rsLighting);
}

void Actor::Save(lsl::SWriter* writer)
{
	_MyBase::Save(writer);

	writer->WriteRef("graph", _graph);
	
	writer->WriteValue("graphLighting", cLightingStr[_graphDesc.lighting]);

	std::stringstream stream;
	bool propsEmpty = true;
	for (int i = 0; i < cGraphPropEnd; ++i)
		if (_graphDesc.props.test(i))
		{
			if (!propsEmpty)
				stream << '\n';
			propsEmpty = false;
			stream << cGraphPropStr[i];
		}

	if (!propsEmpty)
		writer->WriteValue("graphProps", stream.str());

	lsl::SWriteEnum(writer, "graphOrder", _graphDesc.order, cGraphOrderStr, cOrderEnd);

	lsl::SWriteValue(writer, "vec1", _vec1);
	lsl::SWriteValue(writer, "vec2", _vec2);
	lsl::SWriteValue(writer, "vec3", _vec3);
}

void Actor::Load(lsl::SReader* reader)
{
	_MyBase::Load(reader);

	reader->ReadRef("graph", false, this, 0);

	std::string str;
	if (reader->ReadValue("graphLighting", str))
	{
		int val = lsl::ConvStrToEnum(str.c_str(), cLightingStr, cLightingEnd);
		if (val > -1)
			_graphDesc.lighting = Lighting(val);
	}

	if (reader->ReadValue("graphProps", str))
	{
		std::stringstream stream(str);
		while (!stream.eof() && !stream.fail())
		{
			std::string value;
			stream >> value;
			int val = lsl::ConvStrToEnum(value.c_str(), cGraphPropStr, cGraphPropEnd);
			if (val > -1)
				_graphDesc.props.set(val);
		}
	}

	lsl::SReadEnum(reader, "graphOrder", _graphDesc.order, cGraphOrderStr, cOrderEnd);

	lsl::SReadValue(reader, "vec1", _vec1);
	lsl::SReadValue(reader, "vec2", _vec2);
	lsl::SReadValue(reader, "vec3", _vec3);
}

void Actor::OnFixUp(const FixUpNames& fixUpNames)
{
	for (FixUpNames::const_iterator iter = fixUpNames.begin(); iter != fixUpNames.end(); ++iter)
		if (iter->name == "graph")
			SetGraph(iter->GetComponent<GraphManager*>(), _graphDesc);
}

void Actor::ReleaseGraph()
{
	if (_graph)
		_graph->RemoveActor(this);
	_graph = 0;
}

GraphManager* Actor::GetGraph()
{
	return _graph;
}

const Actor::GraphDesc& Actor::GetGraphDesc() const
{
	return _graphDesc;
}

void Actor::SetGraph(GraphManager* graph, const GraphDesc& desc)
{
	ReleaseGraph();

	_graph = graph;
	_graphDesc = desc;

	if (_graph)
		_graph->InsertActor(this);
}

const D3DXVECTOR4& Actor::vec1() const
{
	return _vec1;
}

void Actor::vec1(const D3DXVECTOR4& value)
{
	_vec1 = value;
}

const D3DXVECTOR4& Actor::vec2() const
{
	return _vec2;
}

void Actor::vec2(const D3DXVECTOR4& value)
{
	_vec2 = value;
}

const D3DXVECTOR4& Actor::vec3() const
{
	return _vec3;
}

void Actor::vec3(const D3DXVECTOR4& value)
{
	_vec3 = value;
}

float Actor::texDiffK() const
{
	return _texDiffK;
}

void Actor::texDiffK(float value)
{
	_texDiffK = value;
}

}

}