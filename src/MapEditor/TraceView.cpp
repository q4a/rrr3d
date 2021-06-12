#include "stdafx.h"

#include "TraceView.h"

#include "MainFrm.h"
#include "Resource.h"
#include "MapEditor.h"

class CTraceSplitView: public CView
{
	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CTraceSplitView)

	typedef CView _MyBase;

	static const unsigned cFrameSz = 3;
private:
	CWnd* _srcWnd;
protected:
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs)
	{
		BOOL bRes = CView::PreCreateWindow(cs);
		cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
		return bRes;
	}

	virtual void OnActivateView(BOOL bActivate, CView* pActivateView, CView* pDeactiveView)
	{
		_MyBase::OnActivateView(bActivate, pActivateView, pDeactiveView);

		Invalidate();
	}

	virtual void OnDraw(CDC* pDC) 
	{
		if (GetParentFrame()->GetActiveView() == this)
		{
			CRect rect;
			GetClientRect(rect);
			pDC->FillSolidRect(rect, GetSysColor(COLOR_HIGHLIGHT));
		}
	}

	afx_msg void OnSize(UINT nType, int cx, int cy)
	{
		_MyBase::OnSize(nType, cx, cy);
		
		if (GetSafeHwnd() == NULL || _srcWnd == 0 || _srcWnd->GetSafeHwnd() == 0)
			return;

		CRect rect;
		GetClientRect(rect);
		_srcWnd->SetWindowPos(NULL, rect.left + cFrameSz, rect.top + cFrameSz, rect.Width() - 2*cFrameSz, rect.Height() - 2*cFrameSz, SWP_NOACTIVATE | SWP_NOZORDER);
	}

	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
	{
		BOOL res = _MyBase::OnNotify(wParam, lParam, pResult);

		if (GetOwner())
			res = GetOwner()->SendMessage(WM_NOTIFY, wParam, lParam);

		return res;
	}
public:
	CTraceSplitView(): _srcWnd(0) {}
	virtual ~CTraceSplitView() {}
	
	CWnd* GetSrcWnd() {return _srcWnd;}
	void SetSrcWnd(CWnd* value) {_srcWnd = value;}
};
BEGIN_MESSAGE_MAP(CTraceSplitView, CView)
	ON_WM_SIZE()
END_MESSAGE_MAP()
IMPLEMENT_DYNCREATE(CTraceSplitView, CView)

class CFlatFrameWnd : public CFrameWnd 
{
	DECLARE_DYNAMIC(CFlatFrameWnd);

    virtual BOOL PreCreateWindow(CREATESTRUCT& cs)
	{
		CFrameWnd::PreCreateWindow(cs);
		cs.dwExStyle &= ~WS_EX_CLIENTEDGE;
		return TRUE;
	}
};
IMPLEMENT_DYNAMIC(CFlatFrameWnd, CFrameWnd);

BEGIN_MESSAGE_MAP(CTraceView, CViewPane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_PAINT()
	ON_WM_SETFOCUS()

	ON_COMMAND(IDTB_TRACE_NEWWP, OnCommandNew)
	ON_COMMAND(IDTB_TRACE_DELWP, OnCommandDel)
	ON_COMMAND(IDTB_EDITMAP_NONE, OnEditMapToolBarCommand)

	ON_NOTIFY(TVN_SELCHANGED, TRACE_TREEWP_VIEW, OnPointViewSelChanged)
	ON_NOTIFY(TVN_SELCHANGED, TRACE_TREEPATH_VIEW, OnPathViewSelChanged)	
END_MESSAGE_MAP()




CTraceView::CTraceView(): _mapDoc(0), _flatFrame(0)
{
	_mapDocEvent = new MapDocEvent(this);
}

CTraceView::~CTraceView()
{
	SetMapDoc(0);
	delete _mapDocEvent;
}

int CTraceView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (_MyBase::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	CCreateContext ccc;
	ccc.m_pNewViewClass   = RUNTIME_CLASS(CTraceSplitView);
	ccc.m_pCurrentDoc     = NULL;
	ccc.m_pNewDocTemplate = NULL;
	ccc.m_pLastView       = NULL;
	ccc.m_pCurrentFrame   = NULL;
	// Because the CFRameWnd needs a window class, we will create a new one. 
	CString strMyClass = AfxRegisterWndClass(CS_VREDRAW | CS_HREDRAW, ::LoadCursor(NULL, IDC_ARROW), (HBRUSH) ::GetStockObject(WHITE_BRUSH), ::LoadIcon(NULL, IDI_APPLICATION));
	//
	_flatFrame = new CFlatFrameWnd;
	_flatFrame->Create(strMyClass, _T(""), WS_CHILD | WS_VISIBLE, rectDummy, this);	

	_splitter.CreateStatic(_flatFrame, 2, 1);
	_splitter.ModifyStyleEx(WS_EX_CLIENTEDGE, 0, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
	_splitter.CreateView(0, 0, RUNTIME_CLASS(CTraceSplitView), CSize(100,100), &ccc);
	_splitter.CreateView(1, 0, RUNTIME_CLASS(CTraceSplitView), CSize(100,100), &ccc);

	CTraceSplitView* pView1 = (CTraceSplitView*)_splitter.GetPane(0, 0);
	pView1->SetOwner(this);
	CTraceSplitView* pView2 = (CTraceSplitView*)_splitter.GetPane(1, 0);
	pView2->SetOwner(this);
	//
	_flatFrame->SetActiveView(pView1);




	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;
	if (!_pointView.Create(dwViewStyle | TVS_SHOWSELALWAYS, rectDummy, pView1, TRACE_TREEWP_VIEW))
	{
		TRACE0("Failed to create pointView\n");
		return -1;      // fail to create
	}
	pView1->SetSrcWnd(&_pointView);

	if (!_pathView.Create(dwViewStyle | TVS_SHOWSELALWAYS, rectDummy, pView2, TRACE_TREEPATH_VIEW))
	{
		TRACE0("Failed to create _pathView\n");
		return -1;      // fail to create
	}
	pView2->SetSrcWnd(&_pathView);

	_toolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_TRACE);
	_toolBar.LoadToolBar(IDR_TRACE, 0, 0, TRUE /* Is locked */);
	_toolBar.SetPaneStyle(_toolBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
	_toolBar.SetPaneStyle(_toolBar.GetPaneStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));
	_toolBar.SetOwner(this);
	// All commands will be routed via this control , not via the parent frame:
	_toolBar.SetRouteCommandsViaFrame(FALSE);




	CBitmap bmp;
	bmp.LoadBitmap(IDB_CLASS_VIEW_24);	
	BITMAP bmpObj;
	bmp.GetBitmap(&bmpObj);	
	_images.Create(16, bmpObj.bmHeight, ILC_MASK | ILC_COLOR24, 0, 0);
	_images.Add(&bmp, RGB(255, 0, 0));
	//
	_pointView.SetImageList(&_images, TVSIL_NORMAL);
	_pathView.SetImageList(&_images, TVSIL_NORMAL);

	return 0;
}

void CTraceView::OnDestroy()
{
	SetMapDoc(0);
	delete _flatFrame;
	_flatFrame = 0;

	_MyBase::OnDestroy();
}

void CTraceView::ReleasePointItem(HTREEITEM item)
{
	ForEachTreeCtrlItem(_pointView, item, DelDataFunc(this));
}

void CTraceView::ReleasePointItems()
{
	ForEachTreeCtrlItem(_pointView, DelDataFunc(this));
}

void CTraceView::InsertPointItem(r3d::IWayPointRef waypoint)
{
	CString strId;
	strId.Format(_T("wp %d"), waypoint->GetId());
	HTREEITEM item = _pointView.InsertItem(strId, 1, 1, 0);
	SetPointItemData(item, new PntItemData(waypoint));	
}

void CTraceView::RemovePointItem(HTREEITEM item)
{
	ReleasePointItem(item);
	//При удалении последнего item, странно, но выделение не меняется (Событие OnChangedSelectedItem). Поэтому меняем вручную
	if (_pointView.GetCount() == 1)
		_pointView.SelectItem(0);
	_pointView.DeleteItem(item);	
}

HTREEITEM CTraceView::FindPointItem(const r3d::IWayPointRef& point)
{
	return ForEachTreeCtrlItem(_pointView, FindItemByPointFunc(this, point));
}

void CTraceView::ClearPointList()
{
	LSL_ASSERT(_mapDoc);

	if (_pointView)
	{
		ReleasePointItems();
		_pointView.DeleteAllItems();
	}
}

void CTraceView::UpdatePointList()
{
	LSL_ASSERT(_mapDoc && _pointView);

	ClearPointList();

	r3d::IWayPointRef waypoint = GetTrace()->FirstPoint();
	while (waypoint)
	{
		InsertPointItem(waypoint);
		GetTrace()->NextPoint(waypoint);
	}
}

void CTraceView::ReleasePathItem(HTREEITEM item)
{
	ForEachTreeCtrlItem(_pathView, item, DelPathDataFunc(this));
}

void CTraceView::ReleasePathItems()
{
	ForEachTreeCtrlItem(_pathView, DelPathDataFunc(this));
}

void CTraceView::InsertNodeItem(const r3d::IWayNodeRef& node, HTREEITEM parent)
{
	CString strId;
	strId.Format(_T("node %d"), 0);
	HTREEITEM item = _pathView.InsertItem(strId, 1, 1, parent);
	SetPathItemData(item, new PathItemData(0, node));
}

void CTraceView::InsertPathItem(r3d::IWayPathRef path)
{
	CString strId;
	strId.Format(_T("path %d"), 0);
	HTREEITEM item = _pathView.InsertItem(strId, 2, 2, 0);
	SetPathItemData(item, new PathItemData(path, 0));

	r3d::IWayNodeRef node = path->First();
	while (node)
	{
		InsertNodeItem(node, item);
		path->Next(node);
	}
}

void CTraceView::RemovePathItem(HTREEITEM item)
{
	ReleasePathItem(item);
	//При удалении последнего item, странно, но выделение не меняется (Событие OnChangedSelectedItem). Поэтому меняем вручную
	if (_pathView.GetCount() == 1)
		_pathView.SelectItem(0);
	_pathView.DeleteItem(item);
}

HTREEITEM CTraceView::FindPathItem(const r3d::IWayPathRef& path)
{
	return ForEachTreeCtrlItem(_pathView, FindItemByPathFunc(this, path));
}

void CTraceView::ClearPathList()
{
	LSL_ASSERT(_mapDoc);

	if (_pathView)
	{
		ReleasePathItems();
		_pathView.DeleteAllItems();
	}
}

void CTraceView::UpdatePathList()
{
	LSL_ASSERT(_mapDoc && _pathView);

	ClearPathList();

	r3d::IWayPathRef path = GetTrace()->FirstPath();
	while (path)
	{
		InsertPathItem(path);
		GetTrace()->NextPath(path);
	}
}

void CTraceView::ClearList()
{
	ClearPathList();
	ClearPointList();
}

void CTraceView::UpdateList()
{
	UpdatePointList();
	UpdatePathList();
}

void CTraceView::AddPoint()
{
	r3d::IWayPointRef point = GetTrace()->AddPoint();
	point->SetSize(7.0f);
	InsertPointItem(point);
}

void CTraceView::DelSelPoint()
{
	HTREEITEM item = _pointView.GetSelectedItem();
	if (item)
	{
		r3d::IWayPointRef wp = GetPointItemData(item) ? GetPointItemData(item)->wp : 0;
		if (wp->IsUsedByPath())
		{
			throw lsl::Error("It's used by path");
			return;
		}

		RemovePointItem(item);
		if (wp)
			GetTrace()->DelPoint(wp);
	}
}

void CTraceView::SelectPoint(HTREEITEM item)
{
	LSL_ASSERT(_mapDoc && _pointView);

	_pointView.SelectItem(item);

	PntItemData* data = item ? GetPointItemData(item) : 0;
	PathControlEvent* mEvent = new PathControlEvent(this);
	_mapDoc->SelectWayPoint(data ? data->wp : 0, r3d::ITrace::ControlEventRef(mEvent, mEvent));
}

void CTraceView::SelectPoint(const r3d::IWayPointRef& point)
{
	SelectPoint(FindPointItem(point));
}

void CTraceView::AddPath()
{
	r3d::IWayPathRef path = GetTrace()->AddPath();
	InsertPathItem(path);
}

void CTraceView::DelSelPath()
{
	HTREEITEM item = _pathView.GetSelectedItem();
	if (item)
	{
		PathItemData* data = GetPathItemData(item);
		r3d::IWayPathRef path = data ? data->path : 0;
		r3d::IWayNodeRef node = data ? data->node : 0;
		RemovePathItem(item);		
		if (path)
			GetTrace()->DelPath(path);
		if (node)		
			node->GetPath()->Delete(node);
	}
}

void CTraceView::SelectPath(HTREEITEM item)
{
	LSL_ASSERT(_mapDoc && _pathView);

	_pathView.SelectItem(item);

	PathItemData* data = item ? GetPathItemData(item) : 0;
	GetTrace()->SelectPath(data ? data->path : 0);
}

void CTraceView::SelectPath(const r3d::IWayPathRef& path)
{
	SelectPath(FindPathItem(path));
}

void CTraceView::SelectNode(HTREEITEM item)
{
	LSL_ASSERT(_mapDoc && _pathView);

	_pathView.SelectItem(item);

	PathItemData* data = item ? GetPathItemData(item) : 0;
	GetTrace()->SelectNode(data ? data->node : 0);
}

r3d::ITrace* CTraceView::GetTrace()
{
	r3d::ITrace* trace = theApp.GetWorld()->GetEdit()->GetMap()->GetTrace();

	LSL_ASSERT(trace);

	return trace;
}

CTraceView::PntItemData* CTraceView::GetPointItemData(HTREEITEM item)
{
	LSL_ASSERT(item);

	return reinterpret_cast<PntItemData*>(_pointView.GetItemData(item));
}

CTraceView::PntItemData* CTraceView::GetSelPointItemData()
{
	HTREEITEM item = _pointView.GetSelectedItem();

	return item ? GetPointItemData(item) : 0;
}

void CTraceView::SetPointItemData(HTREEITEM item, PntItemData* value)
{
	LSL_ASSERT(item);

	_pointView.SetItemData(item, DWORD_PTR(value));
}

CTraceView::PathItemData* CTraceView::GetPathItemData(HTREEITEM item)
{
	LSL_ASSERT(item);

	return reinterpret_cast<PathItemData*>(_pathView.GetItemData(item));
}

void CTraceView::SetPathItemData(HTREEITEM item, PathItemData* value)
{
	LSL_ASSERT(item);

	_pathView.SetItemData(item, DWORD_PTR(value));
}

void CTraceView::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)	
		return;

	CRect rectClient;
	GetClientRect(rectClient);

	int cyTlb = _toolBar.CalcFixedLayout(FALSE, TRUE).cy;

	_toolBar.SetWindowPos(NULL, rectClient.left, rectClient.top, rectClient.Width(), cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);	
	_flatFrame->SetWindowPos(NULL, rectClient.left, rectClient.top + cyTlb, rectClient.Width(), rectClient.Height() - cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);
}

bool CTraceView::IsActiveTreeView(CViewTree* view)
{
	LSL_ASSERT(view);

	return _flatFrame->GetActiveView() == view->GetParent();
}

void CTraceView::OnSize(UINT nType, int cx, int cy)
{
	_MyBase::OnSize(nType, cx, cy);

	AdjustLayout();
}

void CTraceView::OnPaint()
{
	_MyBase::OnPaint();

	CPaintDC dc(this); // device context for painting

	CRect rectClient;
	GetClientRect(rectClient);

	//dc.Dr
}

void CTraceView::OnSetFocus(CWnd* pOldWnd)
{
	_MyBase::OnSetFocus(pOldWnd);

	//_pointView.SetFocus();
}

void CTraceView::OnCommandNew()
{
	if (IsActiveTreeView(&_pointView))	
		AddPoint();

	if (IsActiveTreeView(&_pathView))
		AddPath();
}

void CTraceView::OnCommandDel()
{
	if (IsActiveTreeView(&_pointView))
		DelSelPoint();

	if (IsActiveTreeView(&_pathView))
		DelSelPath();
}

void CTraceView::OnEditMapToolBarCommand()
{
}

void CTraceView::OnPointViewSelChanged(NMHDR* arg, LRESULT* res)
{
	if (_mapDoc && _pointView)
		SelectPoint(_pointView.GetSelectedItem());
}

void CTraceView::OnPathViewSelChanged(NMHDR* arg, LRESULT* res)
{
	if (_mapDoc && _pathView)
	{
		SelectPath(_pathView.GetSelectedItem());
		SelectNode(_pathView.GetSelectedItem());
	}
}

void CTraceView::OnChangeActiveTab(bool active)
{
	_MyBase::OnChangeActiveTab(active);

	if (_mapDoc && _pointView)
	{
		GetTrace()->EnableVisualize(active);
		SelectPoint(active ? _pointView.GetSelectedItem() : 0);
	}
}

bool CTraceView::OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord)
{
	if (_mapDoc && _pointView)
	{
		if (key == lsl::mkLeft && state == lsl::ksDown)
		{
			r3d::IWayPointRef point = GetTrace()->PickPoint(coord);
			SelectPoint(point);
			SelectPath(0);
			SelectNode(0);
			return point;
		}
	}

	return false;
}

CMapEditorDoc* CTraceView::GetMapDoc()
{
	return _mapDoc;
}

void CTraceView::SetMapDoc(CMapEditorDoc* value)
{
	if (_mapDoc != value)
	{
		if (_mapDoc)
		{
			SelectPoint(0);
			ClearList();
			_mapDoc->UnregObserver(_mapDocEvent);
		}

		_mapDoc = value;

		if (_mapDoc)
		{
			_mapDoc->RegObserver(_mapDocEvent);
			UpdateList();	
		}
	}
}