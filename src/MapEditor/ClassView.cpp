#include "stdafx.h"

#include "ClassView.h"

#include "MainFrm.h"
#include "Resource.h"
#include "MapEditor.h"

class CClassViewMenuButton : public CMFCToolBarMenuButton
{
	friend class CClassView;

	DECLARE_SERIAL(CClassViewMenuButton)

public:
	CClassViewMenuButton(HMENU hMenu = NULL) : CMFCToolBarMenuButton((UINT)-1, hMenu, -1)
	{
	}

	virtual void OnDraw(CDC* pDC, const CRect& rect, CMFCToolBarImages* pImages, BOOL bHorz = TRUE,
		BOOL bCustomizeMode = FALSE, BOOL bHighlight = FALSE, BOOL bDrawBorder = TRUE, BOOL bGrayDisabledButtons = TRUE)
	{
		pImages = CMFCToolBar::GetImages();

		CAfxDrawState ds;
		pImages->PrepareDrawImage(ds);

		CMFCToolBarMenuButton::OnDraw(pDC, rect, pImages, bHorz, bCustomizeMode, bHighlight, bDrawBorder, bGrayDisabledButtons);

		pImages->EndDrawImage(ds);
	}
};

IMPLEMENT_SERIAL(CClassViewMenuButton, CMFCToolBarMenuButton, 1)

BEGIN_MESSAGE_MAP(CClassView, CViewPane)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_SIZE()
	ON_WM_CONTEXTMENU()
	ON_COMMAND(ID_CLASS_ADD_MEMBER_FUNCTION, OnClassAddMemberFunction)
	ON_COMMAND(ID_CLASS_ADD_MEMBER_VARIABLE, OnClassAddMemberVariable)
	ON_COMMAND(ID_CLASS_DEFINITION, OnClassDefinition)
	ON_COMMAND(ID_CLASS_PROPERTIES, OnClassProperties)
	ON_COMMAND(ID_NEW_FOLDER, OnNewFolder)
	ON_WM_PAINT()
	ON_WM_SETFOCUS()
	ON_COMMAND_RANGE(ID_SORTING_GROUPBYTYPE, ID_SORTING_SORTBYACCESS, OnSort)
	ON_UPDATE_COMMAND_UI_RANGE(ID_SORTING_GROUPBYTYPE, ID_SORTING_SORTBYACCESS, OnUpdateSort)	

	ON_NOTIFY(TVN_SELCHANGED, CLASS_TREE_VIEW, OnViewSelChanged)
END_MESSAGE_MAP()




CClassView::CClassView(): _mapDoc(0), _autoRot(false), _autoScale(false)
{
	m_nCurrSort = ID_SORTING_GROUPBYTYPE;

	_mapDocEvent = new MapDocEvent(this);
}

CClassView::~CClassView()
{
	delete _mapDocEvent;
}

CClassView::MapDocEvent::MapDocEvent(CClassView* view): _view(view)
{
}
		
void CClassView::MapDocEvent::ReleaseDoc()
{
	_view->DeselectItem();
}

void CClassView::ReleaseItem(HTREEITEM item)
{
	ForEachTreeCtrlItem(m_wndClassView, item, DelDataFunc(this));
}

void CClassView::ReleaseItems()
{
	ForEachTreeCtrlItem(m_wndClassView, DelDataFunc(this));
}

void CClassView::InsertItem(const r3d::IRecordNodeRef& node, HTREEITEM parent)
{
	HTREEITEM itemNode = m_wndClassView.InsertItem(StdStrToCString(node->GetName()), 2, 2, parent);
	SetItemData(itemNode, new ItemData(node, 0));	

	r3d::IMapObjRecRef record = node->FirstRecord();
	while (record)
	{
		HTREEITEM itemRec = m_wndClassView.InsertItem(StdStrToCString(record->GetName()), 1, 1, itemNode);
		SetItemData(itemRec, new ItemData(0, record));
		node->NextRecord(record);
	}

	r3d::IRecordNodeRef child = node->FirstNode();
	while (child)
	{
		InsertItem(child, itemNode);
		node->NextNode(child);
	}
}

void CClassView::DelItem(HTREEITEM item)
{
	ReleaseItem(item);

	m_wndClassView.DeleteItem(item);
}

void CClassView::ClearList()
{
	if (_mapDoc && m_wndClassView)
	{
		ReleaseItems();
		m_wndClassView.DeleteAllItems();
	}
}

void CClassView::UpdateList()
{
	LSL_ASSERT(_mapDoc && m_wndClassView);

	ClearList();

	for (unsigned i = 0; i < GetDB()->GetMapObjLibCnt(); ++i)
	{
		r3d::IMapObjLibRef lib = GetDB()->GetMapObjLib(i);
		r3d::IRecordNodeRef node;
		node.Assign(lib);
		
		InsertItem(node, 0);		
	}	
}

void CClassView::SelectItem(const D3DXVECTOR3& mapObjPos)
{
	r3d::IMapObjRecRef oldRec = _newMapObj ? _newMapObj->GetRecord() : 0;
	r3d::IMapObjRecRef curRec = GetSelItemData() ? GetSelItemData()->record : 0;

	if (curRec != oldRec || (curRec && !curRec->Equal(oldRec.Pnt())))
	{
		DeselectItem();

		if (curRec)
		{
			_mapDoc->SetSelMode(r3d::ISceneControl::smLink);

			_newMapObj = _mapDoc->AddMapObj(curRec);
			_newMapObj->SetPos(mapObjPos);
			if (_autoScale)
			{
				_newMapObj->SetScale(IdentityVector * (1.0f + 0.3f * RandomRange(-1.0f, 1.0f)));
			}
			if (_autoRot)
			{
				D3DXQUATERNION rot;
				D3DXQuaternionRotationYawPitchRoll(&rot, D3DX_PI/12 * RandomRange(-1.0f, 1.0f), D3DX_PI/12 * RandomRange(-1.0f, 1.0f), D3DX_PI/12 * RandomRange(-1.0f, 1.0f));
				_newMapObj->SetRot(rot);
			}

			_mapDoc->SelectMapObj(_newMapObj);
		}
	}
}

void CClassView::DeselectItem()
{
	if (_newMapObj)
	{
		//Проверяем, этот ли объект выделен (из-за несинхронности, например передачи фокуса, это может быть не так)
		if (_mapDoc->GetSelMapObj() == _newMapObj)
			_mapDoc->SelectMapObj(0);
		if (_mapDoc->GetSelMode() == r3d::ISceneControl::smLink)
			_mapDoc->SetSelMode(r3d::ISceneControl::smNone);

		_mapDoc->DelMapObj(_newMapObj);
		_newMapObj = 0;
	}
}

r3d::IDataBase* CClassView::GetDB()
{
	r3d::IDataBase* res = theApp.GetWorld()->GetEdit()->GetDB();

	LSL_ASSERT(res);

	return res;
}

CClassView::ItemData* CClassView::GetItemData(HTREEITEM item)
{
	LSL_ASSERT(item);

	return reinterpret_cast<ItemData*>(m_wndClassView.GetItemData(item));
}

void CClassView::SetItemData(HTREEITEM item, ItemData* value)
{
	LSL_ASSERT(item);

	m_wndClassView.SetItemData(item, DWORD_PTR(value));
}

CClassView::ItemData* CClassView::GetSelItemData()
{
	HTREEITEM item = m_wndClassView.GetSelectedItem();

	return item ? GetItemData(item) : 0;
}

int CClassView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (_MyBase::OnCreate(lpCreateStruct) == -1)
		return -1;

	CRect rectDummy;
	rectDummy.SetRectEmpty();

	// Create views:
	const DWORD dwViewStyle = WS_CHILD | WS_VISIBLE | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | TVS_SHOWSELALWAYS;

	if (!m_wndClassView.Create(dwViewStyle, rectDummy, this, CLASS_TREE_VIEW))
	{
		TRACE0("Failed to create Class View\n");
		return -1;      // fail to create
	}

	// Load images:
	m_wndToolBar.Create(this, AFX_DEFAULT_TOOLBAR_STYLE, IDR_SORT);
	m_wndToolBar.LoadToolBar(IDR_SORT, 0, 0, TRUE /* Is locked */);

	OnChangeVisualStyle();

	m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() | CBRS_TOOLTIPS | CBRS_FLYBY);
	m_wndToolBar.SetPaneStyle(m_wndToolBar.GetPaneStyle() & ~(CBRS_GRIPPER | CBRS_SIZE_DYNAMIC | CBRS_BORDER_TOP | CBRS_BORDER_BOTTOM | CBRS_BORDER_LEFT | CBRS_BORDER_RIGHT));

	m_wndToolBar.SetOwner(this);

	// All commands will be routed via this control , not via the parent frame:
	m_wndToolBar.SetRouteCommandsViaFrame(FALSE);

	CMenu menuSort;
	menuSort.LoadMenu(IDR_POPUP_SORT);

	m_wndToolBar.ReplaceButton(ID_SORT_MENU, CClassViewMenuButton(menuSort.GetSubMenu(0)->GetSafeHmenu()));

	CClassViewMenuButton* pButton =  DYNAMIC_DOWNCAST(CClassViewMenuButton, m_wndToolBar.GetButton(0));

	if (pButton != NULL)
	{
		pButton->m_bText = FALSE;
		pButton->m_bImage = TRUE;
		pButton->SetImage(GetCmdMgr()->GetCmdImage(m_nCurrSort));
		pButton->SetMessageWnd(this);
	}

	// Fill in some static tree view data (dummy code, nothing magic here)
	//FillClassView();

	return 0;
}

void CClassView::OnDestroy()
{
	DeselectItem();
	ClearList();

	_MyBase::OnDestroy();
}

void CClassView::OnSize(UINT nType, int cx, int cy)
{
	_MyBase::OnSize(nType, cx, cy);
	AdjustLayout();
}

void CClassView::FillClassView()
{
	HTREEITEM hRoot = m_wndClassView.InsertItem(_T("FakeApp classes"), 0, 0);
	m_wndClassView.SetItemState(hRoot, TVIS_BOLD, TVIS_BOLD);

	HTREEITEM hClass = m_wndClassView.InsertItem(_T("CFakeAboutDlg"), 1, 1, hRoot);
	m_wndClassView.InsertItem(_T("CFakeAboutDlg()"), 3, 3, hClass);

	m_wndClassView.Expand(hRoot, TVE_EXPAND);

	hClass = m_wndClassView.InsertItem(_T("CFakeApp"), 1, 1, hRoot);
	m_wndClassView.InsertItem(_T("CFakeApp()"), 3, 3, hClass);
	m_wndClassView.InsertItem(_T("InitInstance()"), 3, 3, hClass);
	m_wndClassView.InsertItem(_T("OnAppAbout()"), 3, 3, hClass);

	hClass = m_wndClassView.InsertItem(_T("CFakeAppDoc"), 1, 1, hRoot);
	m_wndClassView.InsertItem(_T("CFakeAppDoc()"), 4, 4, hClass);
	m_wndClassView.InsertItem(_T("~CFakeAppDoc()"), 3, 3, hClass);
	m_wndClassView.InsertItem(_T("OnNewDocument()"), 3, 3, hClass);

	hClass = m_wndClassView.InsertItem(_T("CFakeAppView"), 1, 1, hRoot);
	m_wndClassView.InsertItem(_T("CFakeAppView()"), 4, 4, hClass);
	m_wndClassView.InsertItem(_T("~CFakeAppView()"), 3, 3, hClass);
	m_wndClassView.InsertItem(_T("GetDocument()"), 3, 3, hClass);
	m_wndClassView.Expand(hClass, TVE_EXPAND);

	hClass = m_wndClassView.InsertItem(_T("CFakeAppFrame"), 1, 1, hRoot);
	m_wndClassView.InsertItem(_T("CFakeAppFrame()"), 3, 3, hClass);
	m_wndClassView.InsertItem(_T("~CFakeAppFrame()"), 3, 3, hClass);
	m_wndClassView.InsertItem(_T("m_wndMenuBar"), 6, 6, hClass);
	m_wndClassView.InsertItem(_T("m_wndToolBar"), 6, 6, hClass);
	m_wndClassView.InsertItem(_T("m_wndStatusBar"), 6, 6, hClass);

	hClass = m_wndClassView.InsertItem(_T("Globals"), 2, 2, hRoot);
	m_wndClassView.InsertItem(_T("theFakeApp"), 5, 5, hClass);
	m_wndClassView.Expand(hClass, TVE_EXPAND);
}

void CClassView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	CTreeCtrl* pWndTree = (CTreeCtrl*)&m_wndClassView;
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
	CMenu menu;
	menu.LoadMenu(IDR_POPUP_SORT);

	CMenu* pSumMenu = menu.GetSubMenu(0);

	if (AfxGetMainWnd()->IsKindOf(RUNTIME_CLASS(CMDIFrameWndEx)))
	{
		CMFCPopupMenu* pPopupMenu = new CMFCPopupMenu;

		if (!pPopupMenu->Create(this, point.x, point.y, (HMENU)pSumMenu->m_hMenu, FALSE, TRUE))
			return;

		((CMDIFrameWndEx*)AfxGetMainWnd())->OnShowPopupMenu(pPopupMenu);
		UpdateDialogControls(this, FALSE);
	}
}

void CClassView::OnClassAddMemberFunction()
{
	AfxMessageBox(_T("Add member function..."));
}

void CClassView::OnClassAddMemberVariable()
{
	// TODO: Add your command handler code here
}

void CClassView::OnClassDefinition()
{
	// TODO: Add your command handler code here
}

void CClassView::OnClassProperties()
{
	// TODO: Add your command handler code here
}

void CClassView::OnNewFolder()
{
	this->ShowPane(true, true, true);

	//AfxMessageBox(_T("New Folder..."));
}

void CClassView::OnPaint()
{
	CPaintDC dc(this); // device context for painting

	CRect rectTree;
	m_wndClassView.GetWindowRect(rectTree);
	ScreenToClient(rectTree);

	rectTree.InflateRect(1, 1);
	dc.Draw3dRect(rectTree, ::GetSysColor(COLOR_3DSHADOW), ::GetSysColor(COLOR_3DSHADOW));
}

void CClassView::OnSetFocus(CWnd* pOldWnd)
{
	_MyBase::OnSetFocus(pOldWnd);

	m_wndClassView.SetFocus();
}

void CClassView::AdjustLayout()
{
	if (GetSafeHwnd() == NULL)
	{
		return;
	}

	CRect rectClient;
	GetClientRect(rectClient);

	int cyTlb = m_wndToolBar.CalcFixedLayout(FALSE, TRUE).cy;

	m_wndToolBar.SetWindowPos(NULL, rectClient.left, rectClient.top, rectClient.Width(), cyTlb, SWP_NOACTIVATE | SWP_NOZORDER);
	m_wndClassView.SetWindowPos(NULL, rectClient.left + 1, rectClient.top + cyTlb + 1, rectClient.Width() - 2, rectClient.Height() - cyTlb - 2, SWP_NOACTIVATE | SWP_NOZORDER);
}

void CClassView::OnSort(UINT id)
{
	if (m_nCurrSort == id)
	{
		return;
	}

	m_nCurrSort = id;

	CClassViewMenuButton* pButton =  DYNAMIC_DOWNCAST(CClassViewMenuButton, m_wndToolBar.GetButton(0));

	if (pButton != NULL)
	{
		pButton->SetImage(GetCmdMgr()->GetCmdImage(id));
		m_wndToolBar.Invalidate();
		m_wndToolBar.UpdateWindow();
	}
}

void CClassView::OnUpdateSort(CCmdUI* pCmdUI)
{
	pCmdUI->SetCheck(pCmdUI->m_nID == m_nCurrSort);
}

void CClassView::OnViewSelChanged(NMHDR* arg, LRESULT* res)
{
	SelectItem();
}

void CClassView::OnChangeActiveTab(bool active)
{
	_MyBase::OnChangeActiveTab(active);

	active ? SelectItem() : DeselectItem();
}

bool CClassView::OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord)
{
	if (_newMapObj && key == lsl::mkLeft && state == lsl::ksDown)
	{
		D3DXVECTOR3 pos(0, 0, 0);
		if (_newMapObj)
			pos = _newMapObj->GetPos();
		_newMapObj = 0;
		SelectItem(pos);

		return true;
	}

	return false;
}

bool CClassView::OnMapViewKeyEvent(unsigned key, lsl::KeyState state)
{
	if (key == VK_DELETE && state == lsl::ksDown)
		//Занимаем событие удаления, если существует _newMapObj
		return _newMapObj;

	return false;
}

void CClassView::OnChangeVisualStyle()
{
	m_ClassViewImages.DeleteImageList();

	UINT uiBmpId = theApp.m_bHiColorIcons ? IDB_CLASS_VIEW_24 : IDB_CLASS_VIEW;

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

	m_ClassViewImages.Create(16, bmpObj.bmHeight, nFlags, 0, 0);
	m_ClassViewImages.Add(&bmp, RGB(255, 0, 0));

	m_wndClassView.SetImageList(&m_ClassViewImages, TVSIL_NORMAL);

	m_wndToolBar.CleanUpLockedImages();
	m_wndToolBar.LoadBitmap(theApp.m_bHiColorIcons ? IDB_SORT_24 : IDR_SORT, 0, 0, TRUE /* Locked */);
}

CMapEditorDoc* CClassView::GetMapDoc()
{
	return _mapDoc;
}

void CClassView::SetMapDoc(CMapEditorDoc* value)
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

bool CClassView::GetAutoRot() const
{
	return _autoRot;
}

void CClassView::SetAutoRot(bool value)
{
	_autoRot = value;
}

bool CClassView::GetAutoScale() const
{
	return _autoScale;
}

void CClassView::SetAutoScale(bool value)
{
	_autoScale = value;
}