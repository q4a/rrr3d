#include "stdafx.h"
#include "MapEditor.h"

#include "MapEditorDoc.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMapEditorDoc, CDocument)

BEGIN_MESSAGE_MAP(CMapEditorDoc, CDocument)
END_MESSAGE_MAP()




CMapEditorDoc::CMapEditorDoc()
{
}

CMapEditorDoc::~CMapEditorDoc()
{
}

void CMapEditorDoc::ClearSelection()
{
	_selMapObj = 0;
	_selWayPoint = 0;

	if (theApp.GetWorld())
		GetScControl()->SelectNode(0);
}

void CMapEditorDoc::ReleaseDoc()
{
	ClearSelection();

	for (ObserverList::iterator iter = _observerList.begin(); iter != _observerList.end(); ++iter)
		(*iter)->ReleaseDoc();
}

void CMapEditorDoc::UpdateDoc()
{
	for (ObserverList::iterator iter = _observerList.begin(); iter != _observerList.end(); ++iter)
		(*iter)->OnUpdate();
}

r3d::IEdit* CMapEditorDoc::GetEdit()
{
	r3d::IEdit* res = theApp.GetWorld()->GetEdit();

	LSL_ASSERT(res);

	return res;
}

r3d::IMap* CMapEditorDoc::GetMap()
{
	r3d::IMap* res = GetEdit()->GetMap();

	LSL_ASSERT(res);

	return res;
}

r3d::ITrace* CMapEditorDoc::GetTrace()
{
	r3d::ITrace* res = GetMap()->GetTrace();

	LSL_ASSERT(res);

	return res;
}

r3d::ISceneControl* CMapEditorDoc::GetScControl()
{
	r3d::ISceneControl* res = GetEdit()->GetScControl();

	LSL_ASSERT(res);

	return res;
}

BOOL CMapEditorDoc::OnNewDocument()
{
	ReleaseDoc();
	BOOL res =  _MyBase::OnNewDocument();
	GetMap()->ClearMap();
	GetTrace()->Clear();
	UpdateDoc();

	SetModifiedFlag(true);

	return res;
}

BOOL CMapEditorDoc::OnSaveDocument(LPCTSTR lpszPathName)
{
	BOOL res = _MyBase::OnSaveDocument(lpszPathName);
	theApp.GetWorld()->SaveLevel(CStringToStdStr(lpszPathName));

	SetModifiedFlag(true);

	return res;
}

BOOL CMapEditorDoc::OnOpenDocument(LPCTSTR lpszPathName)
{
	ReleaseDoc();
	BOOL res = _MyBase::OnOpenDocument(lpszPathName);
	theApp.GetWorld()->LoadLevel(CStringToStdStr(lpszPathName));
	UpdateDoc();

	SetModifiedFlag(true);

	return res;
}

void CMapEditorDoc::OnCloseDocument()
{
	ReleaseDoc();

	_MyBase::OnCloseDocument();
}

void CMapEditorDoc::Serialize(CArchive& ar)
{
	if (ar.IsStoring())
	{
		// TODO: add storing code here
	}
	else
	{
		// TODO: add loading code here
	}
}

void CMapEditorDoc::RegObserver(MapDocObserver* observer)
{
	_observerList.Insert(observer);
	observer->AddRef();
}

void CMapEditorDoc::UnregObserver(MapDocObserver* observer)
{
	_observerList.Remove(observer);
	observer->Release();
}

r3d::IMapObjRef CMapEditorDoc::AddMapObj(const r3d::IMapObjRecRef& record)
{
	r3d::IMapObjRef res = GetMap()->AddMapObj(record);
	
	for (ObserverList::iterator iter = _observerList.begin(); iter != _observerList.end(); ++iter)
		(*iter)->OnAddMapObj(res);

	return res;
}

void CMapEditorDoc::DelMapObj(r3d::IMapObjRef& ref)
{
	for (ObserverList::iterator iter = _observerList.begin(); iter != _observerList.end(); ++iter)
		(*iter)->OnDelMapObj(ref);

	GetMap()->DelMapObj(ref);
}

r3d::IMapObjRef CMapEditorDoc::PickMapObj(const lsl::Point& coord)
{
	return GetMap()->PickMapObj(coord);
}

std::string CMapEditorDoc::GetCatName(unsigned i)
{
	return GetMap()->GetCatName(i);
}

unsigned CMapEditorDoc::GetCatCount()
{
	return GetMap()->GetCatCount();
}

r3d::IMapObjRef CMapEditorDoc::GetFirst(unsigned cat)
{
	return GetMap()->GetFirst(cat);
}

void CMapEditorDoc::GetNext(unsigned cat, r3d::IMapObjRef& ref)
{
	GetMap()->GetNext(cat, ref);
}

r3d::IMapObjRef CMapEditorDoc::GetSelMapObj()
{
	return _selMapObj;
}

void CMapEditorDoc::SelectMapObj(const r3d::IMapObjRef& value)
{
	//Очищаем все выделения только если задан аргумент текущего выделения
	if (_selMapObj != value && (!_selMapObj || !_selMapObj->Equal(value.Pnt())))
	{
		ClearSelection();

		_selMapObj = value;
		MapContrEvent* mEvent = new MapContrEvent(this);
		r3d::IScNodeContRef control = _selMapObj ? GetMap()->GetMapObjControl(_selMapObj, r3d::IMap::ControlEventRef(mEvent, mEvent)) : 0;
		GetScControl()->SelectNode(control);

		for (ObserverList::iterator iter = _observerList.begin(); iter != _observerList.end(); ++iter)
			(*iter)->OnSelectMapObj(value);
	}
}

r3d::IWayPointRef CMapEditorDoc::GetSelWayPoint()
{
	return _selWayPoint;
}

void CMapEditorDoc::SelectWayPoint(const r3d::IWayPointRef& point, const r3d::ITrace::ControlEventRef& mEvent)
{
	//Очищаем все выделения только если задан аргумент текущего выделения
	if (_selWayPoint != point && (!_selWayPoint || !_selWayPoint->Equal(point.Pnt())))
	{
		ClearSelection();
		
		_selWayPoint = point;
		r3d::IScNodeContRef control = _selWayPoint ? GetTrace()->GetPointControl(_selWayPoint, mEvent) : 0;
		GetScControl()->SelectNode(control);
	}
}

r3d::ISceneControl::SelMode CMapEditorDoc::GetSelMode()
{
	return GetScControl()->GetSelMode();
}

void CMapEditorDoc::SetSelMode(r3d::ISceneControl::SelMode value)
{
	GetScControl()->SetSelMode(value);
}

#ifdef _DEBUG
void CMapEditorDoc::AssertValid() const
{
	CDocument::AssertValid();
}

void CMapEditorDoc::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif