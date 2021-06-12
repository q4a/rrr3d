#include "stdafx.h"

#include "game\TraceGfx.h"

namespace r3d
{

namespace game
{

TraceGfx::TraceGfx(Trace* trace): _trace(trace), _selPoint(0), _selPath(0), _selNode(0), _pointLink(0)
{
	LSL_ASSERT(_trace);

	_trace->AddRef();
	
	{
		_libMat = new graph::LibMaterial();
		graph::Material& mat = _libMat->material;
		mat.SetBlending(graph::Material::bmTransparency);
		mat.SetDiffuse(clrRed);
		mat.SetAlpha(0.5f);
		mat.SetOption(graph::Material::moLighting, false);
		mat.SetOption(graph::Material::moZWrite, false);
		mat.SetOption(graph::Material::moZTest, false);
		mat.SetOption(graph::Material::moIgnoreFog, true);
	}

	_wayPnt = new graph::Box();	
	_wayPnt->material.Set(_libMat);

	_sprite = new graph::Sprite();
	_sprite->fixDirection = true;
	_sprite->material.Set(_libMat);
}

TraceGfx::~TraceGfx()
{
	SetPointLink(0);
	SetSelNode(0);
	SetSelPath(0);
	SetSelPoint(0);

	delete _sprite;
	delete _wayPnt;
	delete _libMat;

	_trace->Release();
}

void TraceGfx::DrawNodes(graph::Engine& engine, D3DXVECTOR3* vBuf, unsigned triCnt, const D3DXCOLOR& color)
{
	_libMat->material.SetDiffuse(color);
	_libMat->Apply(engine);

	engine.BeginDraw();
	engine.GetContext().SetWorldMat(IdentityMatrix);
	engine.GetContext().SetRenderState(graph::rsCullMode, D3DCULL_NONE);

	engine.GetDriver().GetDevice()->SetFVF(D3DFVF_XYZ);
	engine.GetDriver().GetDevice()->DrawPrimitiveUP(D3DPT_TRIANGLESTRIP, triCnt, vBuf, sizeof(D3DXVECTOR3));

	engine.GetContext().RestoreRenderState(graph::rsCullMode);
	engine.EndDraw(false);
	
	_libMat->UnApply(engine);
}

void TraceGfx::DoRender(graph::Engine& engine)
{
	//Отрисовка путей
	for (Trace::Points::const_iterator iter = _trace->GetPoints().begin(); iter != _trace->GetPoints().end(); ++iter)
	{
		_libMat->material.SetDiffuse(*iter == _selPoint ? clrGreen : clrRed);

		_wayPnt->SetPos((*iter)->GetPos());
		_wayPnt->SetScale((*iter)->GetSize());			
		_wayPnt->Render(engine);		
	}

	//Отрисовка связей между путями
	Vec3Range resColor(D3DXVECTOR3(clrWhite / 2.0f), D3DXVECTOR3(clrWhite), Vec3Range::vdVolume);
	D3DXVECTOR3 upVec = engine.GetContext().GetCamera().GetDesc().dir;
	float iPath = 0.0f;
	float pathCnt = static_cast<float>(_trace->GetPathes().size());
	
	for (Trace::Pathes::const_iterator iter = _trace->GetPathes().begin(); iter != _trace->GetPathes().end(); ++iter, ++iPath)
	{
		WayPath* path = *iter;
		D3DXCOLOR pathColor = path == _selPath ? clrGreen : D3DXCOLOR(resColor.GetValue(iPath / (pathCnt - 1.0f)));
		res::VertexData vBuf;
		path->GetTriStripVBuf(vBuf, &upVec);
		if (vBuf.IsInit())
			DrawNodes(engine, vBuf.begin().Pos3(), vBuf.GetVertexCount() - 2, pathColor);
	}

	//Для выделенного узла свой цикл отрисовки
	if (_selNode && _selNode->GetNext())
	{
		D3DXVECTOR3 vBuf[4];
		_selNode->GetTile().GetVBuf(vBuf, 4, &upVec);
		DrawNodes(engine, vBuf, 4 - 2, clrGreen);
	}

	//Отрисовка связи выделения
	if (_pointLink)
	{
		D3DXVECTOR3 pos1 = _pointLink->GetPoint()->GetPos();
		D3DXVECTOR3 pos2 = _pointLink->GetPos();
		D3DXVECTOR3 dir = pos1 - pos2;
		D3DXVECTOR2 sizes(D3DXVec3Length(&dir), _pointLink->GetPoint()->GetSize());
		D3DXVec3Normalize(&dir, &dir);

		_sprite->SetPos((pos1 + pos2) / 2.0f);
		_sprite->SetDir(dir);
		_sprite->sizes = sizes;

		_libMat->material.SetDiffuse(clrGreen);
		_sprite->Render(engine);
	}
}

WayPoint* TraceGfx::GetSelPoint()
{
	return _selPoint;
}

void TraceGfx::SetSelPoint(WayPoint* value)
{
	if (lsl::Object::ReplaceRef(_selPoint, value))
	{
		_selPoint = value;
	}
}

WayPath* TraceGfx::GetSelPath()
{
	return _selPath;
}

void TraceGfx::SetSelPath(WayPath* value)
{
	_selPath = value;
}

WayNode* TraceGfx::GetSelNode()
{
	return _selNode;
}

void TraceGfx::SetSelNode(WayNode* value)
{
	if (ReplaceRef(_selNode, value))
		_selNode = value;
}

TraceGfx::PointLink* TraceGfx::GetPointLink()
{
	return _pointLink;
}

void TraceGfx::SetPointLink(PointLink* value)
{
	if (ReplaceRef(_pointLink, value))
		_pointLink = value;
}

}

}