#include "stdafx.h"
#include "MapEditor.h"

#include "MapEditorDoc.h"
#include "MapEditorView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMapEditorView, CView)

BEGIN_MESSAGE_MAP(CMapEditorView, CView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()

	ON_WM_SIZE()
END_MESSAGE_MAP()




CMapEditorView::CMapEditorView()
{
}

CMapEditorView::~CMapEditorView()
{
}

int CMapEditorView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	unsigned res = _MyBase::OnCreate(lpCreateStruct);

	theApp.RegR3DView(this);

	return res;
}

void CMapEditorView::OnDestroy()
{
	theApp.GetMainFrame()->SetActiveMapView(0);

	theApp.UnRegR3DView(this);

	_MyBase::OnDestroy();
}

void CMapEditorView::OnActivateFrame(UINT nState, CFrameWnd* pFrameWnd)
{
	theApp.GetMainFrame()->SetActiveMapView(this);

	_MyBase::OnActivateFrame(nState, pFrameWnd);
}

void CMapEditorView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	_MyBase::OnUpdate(pSender, lHint, pHint);
}

void CMapEditorView::OnMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord, bool shift, bool ctrl)
{
	if (!GetDocument())
		return;

	bool res = GetR3DView()->OnMouseClickEvent(key, state, coord, shift, ctrl);
	res = res || theApp.GetMainFrame()->OnMapViewMouseClickEvent(key, state, coord);

	if (!res && key == lsl::mkLeft && state == lsl::ksDown)
	{
		GetDocument()->SelectMapObj(GetDocument()->PickMapObj(coord));
	}
}

void CMapEditorView::OnMouseMoveEvent(const lsl::Point& coord, bool shift, bool ctrl)
{
	GetR3DView()->OnMouseMoveEvent(coord, shift, ctrl);
	theApp.GetMainFrame()->OnMapViewMouseMoveEvent(coord);
}

void CMapEditorView::OnKeyEvent(unsigned key, lsl::KeyState state)
{
	bool res = GetR3DView()->OnKeyEvent(key, state, false);
	res = res || theApp.GetMainFrame()->OnMapViewKeyEvent(key, state);

	if (!res && key == VK_DELETE && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = GetDocument()->GetSelMapObj();
		if (ref)
		{
			GetDocument()->SelectMapObj(0);
			GetDocument()->DelMapObj(ref);
		}
	}
}

void CMapEditorView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	OnKeyEvent(nChar, lsl::ksDown);
}

void CMapEditorView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	OnKeyEvent(nChar, lsl::ksUp);
}

void CMapEditorView::OnLButtonDown(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkLeft, lsl::ksDown, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false, (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnLButtonUp(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkLeft, lsl::ksUp, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false, (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnRButtonDown(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkRight, lsl::ksDown, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false, (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnRButtonUp(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkRight, lsl::ksUp, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false, (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnMouseMove(UINT nFlags, CPoint point)
{
	OnMouseMoveEvent(lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false, (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}

void CMapEditorView::OnSize(UINT nType, int cx, int cy)
{
	if (GetR3DView() && cx > 100 && cy > 100)
	{
		r3d::IView::Desc desc;
		desc.fullscreen = false;
		desc.resolution = lsl::Point(cx, cy);
		desc.handle = GetSafeHwnd();		
		GetR3DView()->Reset(desc);

		//GetR3DView()->SetCameraAspect(static_cast<float>(cx) / cy);
	}
}

r3d::IView* CMapEditorView::GetR3DView()
{
	return theApp.GetWorld()->GetView();
}

BOOL CMapEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

void CMapEditorView::OnDraw(CDC* /*pDC*/)
{
	CMapEditorDoc* pDoc = GetDocument();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;
}

#ifdef _DEBUG
void CMapEditorView::AssertValid() const
{
	CView::AssertValid();
}

void CMapEditorView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMapEditorDoc* CMapEditorView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMapEditorDoc)));
	return (CMapEditorDoc*)m_pDocument;
}
#endif