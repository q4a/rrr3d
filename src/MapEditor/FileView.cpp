#include "stdafx.h"
#include "mainfrm.h"
#include "FileView.h"
#include "Resource.h"
#include "MapEditor.h"

#ifdef _DEBUG
#undef THIS_FILE
static char THIS_FILE[]=__FILE__;
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CFileView, CViewPane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_PROPERTIES, OnProperties)
	ON_COMMAND(ID_OPEN, OnFileOpen)
	ON_COMMAND(ID_OPEN_WITH, OnFileOpenWith)
	ON_COMMAND(ID_DUMMY_COMPILE, OnDummyCompile)
	ON_COMMAND(ID_EDIT_CUT, OnEditCut)
	ON_COMMAND(ID_EDIT_COPY, OnEditCopy)
	ON_COMMAND(ID_EDIT_CLEAR, OnEditClear)
	ON_WM_PAINT()
	ON_WM_SETFOCUS()

	ON_NOTIFY(TVN_SELCHANGED, FILE_TREE_VIEW, OnViewSelChanged)
	ON_NOTIFY(NM_KEYDOWN, FILE_TREE_VIEW, OnViewKeyDown)
END_MESSAGE_MAP()




CFileView::CFileView(): _mapDoc(0)
{
	_mapDocEvent = new MapDocEvent(this);
}

CFileView::~CFileView()
{
	delete _mapDocEvent;
}

CFileView::MapDocEvent::MapDocEvent(CFileView* view): _view(view)
{
}

void CFileView::MapDocEvent::OnAddMapObj(const r3d::IMapObjRef& ref)
{
	if (_view->_mapDoc && _view->m_wndFileView)
		_view->InsertItemCat(ref);
}

void CFileView::MapDocEvent::OnDelMapObj(const r3d::IMapObjRef& ref)
{
	if (_view->_mapDoc && _view->m_wndFileView)
		_view->RemoveItem(ref);
}

void CFileView::MapDocEvent::OnSelectMapObj(const r3d::IMapObjRef& ref)
{
	if (_view->_mapDoc && _view->m_wndFileView)
		_view->SelectItem(ref);
}

void CFileView::MapDocEvent::OnUpdate()
{
	if (_view->_mapDoc && _view->m_wndFileView)
		_view->UpdateList();
}

void CFileView::MapDocEvent::ReleaseDoc()
{
	if (_view->_mapDoc && _view->m_wndFileView)
		_view->ClearList();
}

void CFileView::ReleaseItem(HTREEITEM item)
{
	ForEachTreeCtrlItem(m_wndFileView, item, DelDataFunc(this));
}

void CFileView::ReleaseItems()
{
	ForEachTreeCtrlItem(m_wndFileView, DelDataFunc(this));
}

void CFileView::InsertItem(const r3d::IMapObjRef& mapObj, HTREEITEM parent)
{
	HTREEITEM item = m_wndFileView.InsertItem(StdStrToCString(mapObj->GetName()), 2, 2, parent);
	SetItemData(item, new ItemData(mapObj, -1));
}

void CFileView::InsertItemCat(const r3d::IMapObjRef& mapObj)
{
	unsigned catInd = mapObj->GetRecord()->GetCategory();
	HTREEITEM catItem = FindCatItem(catInd);

	LSL_ASSERT(catItem);

	InsertItem(mapObj, catItem);
}

void CFileView::RemoveItem(const r3d::IMapObjRef& mapObj)
{
	HTREEITEM item = FindMapObjItem(mapObj);
	if (item)
	{
		ReleaseItem(item);
		m_wndFileView.DeleteItem(item);
	}
}

void CFileView::InsertCategory(unsigned i)
{
	HTREEITEM itemCat = m_wndFileView.InsertItem(StdStrToCString(_mapDoc->GetCatName(i)), 0, 0, 0);
	SetItemData(itemCat, new ItemData(0, i));
	
	r3d::IMapObjRef mapObj = _mapDoc->GetFirst(i);
	while (mapObj)
	{
		InsertItem(mapObj, itemCat);

		_mapDoc->GetNext(i, mapObj);
	}
}

HTREEITEM CFileView::FindCatItem(unsigned i)
{
	HTREEITEM item = m_wndFileView.GetRootItem();
	while (item)
	{
		ItemData* data = GetItemData(item);
		LSL_ASSERT(data);
		if (data->catIndex == i)
			return item;
		
		item = m_wndFileView.GetNextSiblingItem(item);
	}

	return 0;
}

HTREEITEM CFileView::FindMapObjItem(const r3d::IMapObjRef& mapObj)
{
	return ForEachTreeCtrlItem(m_wndFileView, FindItemByMapObjFunc(this, mapObj));
}

void CFileView::ClearList()
{
	if (_mapDoc && m_wndFileView)
	{
		ReleaseItems();
		m_wndFileView.DeleteAllItems();
	}
}

void CFileView::UpdateList()
{
	LSL_ASSERT(_mapDoc && m_wndFileView);

	ClearList();

	for (unsigned i = 0; i < _mapDoc->GetCatCount(); ++i)
	{
		 InsertCategory(i);
	}
}

void CFileView::SelectItem(HTREEITEM item)
{
	LSL_ASSERT(_mapDoc && m_wndFileView);

	if (item)
	{
		ItemData* selItem = GetItemData(item);
		m_wndFileView.SelectItem(item);
	}
	else	
		DeselectItem();
}

void CFileView::SelectItem(const r3d::IMapObjRef& mapObj)
{
	HTREEITEM item = FindMapObjItem(mapObj);
	if (item)
		SelectItem(item);
	else
	{
		ItemData* itemData = GetSelItemData();
		//—брасываем выделение только если выеделенный узел есть запись о mapObj, чтобы не сбрасывать веделение папок
		if (itemData && itemData->mapObj)
			DeselectItem();
	}
}

void CFileView::DeselectItem()
{
	m_wndFileView.SelectItem(0);
}

CFileView::ItemData* CFileView::GetItemData(HTREEITEM item)
{
	LSL_ASSERT(item);

	return reinterpret_cast<ItemData*>(m_wndFileView.GetItemData(item));
}

void CFileView::SetItemData(HTREEITEM item, ItemData* value)
{
	LSL_ASSERT(item);

	m_wndFileView.SetItemData(item, DWORD_PTR(value));
}

CFileView::ItemData* CFileView::GetSelItemData()
{
	HTREEITEM item = m_wndFileView.GetSelectedItem();

	return item ? GetItemData(item) : 0;
}

void CFileView::FillFileView()
{
	HTREEITEM hRoot = m_wndFileView.InsertItem(_T("FakeApp files"), 0, 0);
	m_wndFileView.SetItemState(hRoot, TVIS_BOLD, TVIS_BOLD);

	HTREEITEM hSrc = m_wndFileView.InsertItem(_T("FakeApp Source Files"), 0, 0, hRoot);

	m_wndFileView.InsertItem(_T("FakeApp.cpp"), 1, 1, hSrc);
	m_wndFileView.InsertItem(_T("FakeApp.rc"), 1, 1, hSrc);
	m_wndFileView.InsertItem(_T("FakeAppDoc.cpp"), 1, 1, hSrc);
	m_wndFileView.InsertItem(_T("FakeAppView.cpp"), 1, 1, hSrc);
	m_wndFileView.InsertItem(_T("MainFrm.cpp"), 1, 1, hSrc);
	m_wndFileView.InsertItem(_T("StdAfx.cpp"), 1, 1, hSrc);

	HTREEITEM hInc = m_wndFileView.InsertItem(_T("FakeApp Header Files"), 0, 0, hRoot);

	m_wndFileView.InsertItem(_T("FakeApp.h"), 2, 2, hInc);
	m_wndFileView.InsertItem(_T("FakeAppDoc.h"), 2, 2, hInc);
	m_wndFileView.InsertItem(_T("FakeAppView.h"), 2, 2, hInc);
	m_wndFileView.InsertItem(_T("Resource.h"), 2, 2, hInc);
	m_wndFileView.InsertItem(_T("MainFrm.h"), 2, 2, hInc);
	m_wndFileView.InsertItem(_T("StdAfx.h"), 2, 2, hInc);

	HTREEITEM hRes = m_wndFileView.InsertItem(_T("FakeApp Resource Files"), 0, 0, hRoot);

	m_wndFileView.InsertItem(_T("FakeApp.ico"), 2, 2, hRes);
	m_wndFileView.InsertItem(_T("FakeApp.rc2"), 2, 2, hRes);
	m_wndFileView.InsertItem(_T("FakeAppDoc.ico"), 2, 2, hRes);
	m_wndFileView.InsertItem(_T("FakeToolbar.bmp"), 2, 2, hRes);

	m_wndFileView.Expand(hRoot, TVE_EXPAND);
	m_wndFileView.Expand(hSrc, TVE_EXPAND);
	m_wndFileView.Expand(hInc, TVE_EXPAND);
}


int CFileView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (_MyBase::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// Create view:
	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | TVS_SHOWSELALWAYS;

	if (!m_wndFileView.Create(dwViewStyle, rectDummy, this, FILE_TREE_VIEW))
	{
		TRACE0("Failed to create file view\n");
		return -1;      // fail to create
	}

	// Load view images:
	m_FileViewImages.Create(IDB_FILE_VIEW, 16, 0, RGB(255, 0, 255));
	m_wndFileView.SetImageList(&m_FileViewImages, TVSIL_NORMAL);

	m_wndToolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_EXPLORER);
	m_wndToolBar.LoadToolBar(IDR_EXPLORER, 0, 0, TRUE /* Is locked */);

	OnChangeVisualStyle();

	m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);

	m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));

	m_wndToolBar.SetOwner(this);

	// All commands will be routed via this control , not via the parent frame:
	m_wndToolBar.SetRouteCommandsViaFrame(FALSE);

	// Fill in some static tree view data (dummy code, nothing magic here)
	FillFileView();
	AdjustLayout();

	return 0;
}

void CFileView::OnDestroy()
{
	DeselectItem();
	ClearList();

	_MyBase::OnDestroy();
}

void CFileView::OnSize(UINT nType, int cx, int cy)
{
	_MyBase::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CFileView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CTreeCtrl* pWndTree = (CTreeCtrl*) &m_wndFileView;
	ASSERT_VALID(pWndTree);

	if (pWnd != pWndTree)
	{
		_MyBase::OnContextMenu(pWnd, point);
		return;
	}

	if (point != CPoint(-1, -1))
	{
		// Select clicked item:
		CPoint ptTree = point;
		pWndTree->ScreenToClient(&ptTree);

		UINT flags = 0;
		HTREEITEM hTreeItem = pWndTree->HitTest(ptTree, &flags);
		if (hTreeItem != NULL)
		{
			pWndTree->SelectItem(hTreeItem);
		}
	}

	pWndTree->SetFocus();
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EXPLORER, point.x, point.y, this, TRUE);
}

void CFileView::OnProperties()
{
	AfxMessageBox(_T("Properties...."));

}

void CFileView::OnFileOpen()
{
	// TODO: Add your command handler code here
}

void CFileView::OnFileOpenWith()
{
	// TODO: Add your command handler code here
}

void CFileView::OnDummyCompile()
{
	// TODO: Add your command handler code here
}

void CFileView::OnEditCut()
{
	// TODO: Add your command handler code here
}

void CFileView::OnEditCopy()
{
	// TODO: Add your command handler code here
}

void CFileView::OnEditClear()
{
	// TODO: Add your command handler code here
}

void CFileView::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rectTree;
	m_wndFileView.GetWindowRect(rectTree);
	ScreenToClient(rectTree);

	rectTree.InflateRect(1, 1);
	dc.Draw3dRect(rectTree, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CFileView::OnSetFocus(CWnd* pOldWnd)
{
	_MyBase::OnSetFocus(pOldWnd);

	m_wndFileView.SetFocus();
}

void CFileView::OnViewSelChanged(NMHDR* arg, LRESULT* res)
{
	if (_mapDoc && m_wndFileView)
	{
		ItemData* itemData = GetSelItemData();
		_mapDoc->SelectMapObj(itemData ? itemData->mapObj : 0);
	}
}

void CFileView::OnViewKeyDown(NMHDR* arg, LRESULT* res)
{
	NMKEY* keyArg = (NMKEY*)arg;
	switch (keyArg->nVKey)
	{
	case VK_DELETE:
	{
		ItemData* itemData = GetSelItemData();
		if (itemData && itemData->mapObj)
		{
			r3d::IMapObjRef mapObj = itemData->mapObj;
			_mapDoc->SelectMapObj(0);
			_mapDoc->DelMapObj(mapObj);
		}
		break;
	}

	}
}

void CFileView::OnChangeActiveTab(bool active)
{
	_MyBase::OnChangeActiveTab(active);
}

void CFileView::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	int cyTlb = m_wndToolBar.CalcFixedLayout(FALSE, TRUE).cy;

	m_wndToolBar.SetWindowPos(NULL, rectClient.left, rectClient.top, rectClient.Width(), cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);
	m_wndFileView.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + cyTlb + 1, rectClient.Width() - 2, rectClient.Height() - cyTlb - 2, SWP_NOACTIVATE | SWP_NOZORDER);
}

void CFileView::OnChangeVisualStyle()
{
	m_wndToolBar.CleanUpLockedImages();
	m_wndToolBar.LoadBitmap(theApp.m_bHiColorIcons ? IDB_EXPLORER_24 : IDR_EXPLORER, 0, 0, TRUE /* Locked */);

	m_FileViewImages.DeleteImageList();

	UINT uiBmpId = theApp.m_bHiColorIcons ? IDB_FILE_VIEW_24 : IDB_FILE_VIEW;

	CBitmap bmp;
	if (!bmp.LoadBitmap(uiBmpId))
	{
		TRACE(_T("Can't load bitmap: %x\n"), uiBmpId);
		ASSERT(FALSE);
		return;
	}

	BITMAP bmpObj;
	bmp.GetBitmap(&bmpObj);

	UINT nFlags = ILC_MASK;

	nFlags |= (theApp.m_bHiColorIcons) ? ILC_COLOR24 : ILC_COLOR4;

	m_FileViewImages.Create(16, bmpObj.bmHeight, nFlags, 0, 0);
	m_FileViewImages.Add(&bmp, RGB(255, 0, 255));

	m_wndFileView.SetImageList(&m_FileViewImages, TVSIL_NORMAL);
}

CMapEditorDoc* CFileView::GetMapDoc()
{
	return _mapDoc;
}

void CFileView::SetMapDoc(CMapEditorDoc* value)
{
	if (_mapDoc != value)
	{
		if (_mapDoc)
		{
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