#include "stdafx.h"

#include "MapEditor.h"
#include "MainFrm.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMainFrame, CFrameWndEx)

BEGIN_MESSAGE_MAP(CMainFrame, CFrameWndEx)
	ON_WM_CREATE()
	ON_COMMAND(ID_VIEW_CUSTOMIZE, &CMainFrame::OnViewCustomize)
	ON_REGISTERED_MESSAGE(AFX_WM_CREATETOOLBAR, &CMainFrame::OnToolbarCreateNew)

	ON_COMMAND_RANGE(IDTB_EDITMAP_NONE, ID_CAM_LIGHT, OnEditMapToolBarCommand)
	ON_UPDATE_COMMAND_UI_RANGE(IDTB_EDITMAP_NONE, ID_CAM_LIGHT, OnEditMapToolBarUpdateCommandUI)
END_MESSAGE_MAP()

static UINT indicators[] =
{
	ID_SEPARATOR,           
	ID_INDICATOR_CAPS,
	ID_INDICATOR_NUM,
	ID_INDICATOR_SCRL,
};




CMainFrame::CMainFrame(): _activeMapView(0), _activeViewPane(0)
{
	_viewPaneList.push_back(&m_wndClassView);
	_viewPaneList.push_back(&m_wndFileView);
	_viewPaneList.push_back(&_traceView);
}

CMainFrame::~CMainFrame()
{
	_viewPaneList.clear();
}

CMapEditorDoc* CMainFrame::GetMapDoc()
{
	return _activeMapView ? _activeMapView->GetDocument() : 0;
}

BOOL CMainFrame::PreCreateWindow(CREATESTRUCT& cs)
{
	if( !CFrameWndEx::PreCreateWindow(cs) )
		return FALSE;
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return TRUE;
}

BOOL CMainFrame::CreateDockingWindows()
{
	BOOL bNameValid;

	// Create class view
	CString strClassView;
	bNameValid = strClassView.LoadString(IDS_CLASS_VIEW);
	ASSERT(bNameValid);
	if (!m_wndClassView.Create(strClassView, this, CRect(0, 0, 200, 200), TRUE, ID_VIEW_CLASSVIEW, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT | CBRS_FLOAT_MULTI))
	{
		TRACE0("Failed to create Class View window\n");
		return FALSE; // failed to create
	}

	// Create file view
	CString strFileView;
	bNameValid = strFileView.LoadString(IDS_FILE_VIEW);
	ASSERT(bNameValid);
	if (!m_wndFileView.Create(strFileView, this, CRect(0, 0, 200, 200), TRUE, ID_VIEW_FILEVIEW, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT| CBRS_FLOAT_MULTI))
	{
		TRACE0("Failed to create File View window\n");
		return FALSE; // failed to create
	}

	SetDlgCtrlID(0);

	//Create Trace View
	CString strTraceName;
	bNameValid = strTraceName.LoadString(IDS_TRACEVIEW);
	ASSERT(bNameValid);		
	if (!_traceView.Create(strTraceName, this, CRect(0, 0, 200, 200), TRUE, ID_VIEW_TRACEVIEW, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT | CBRS_FLOAT_MULTI))
	{
		TRACE0("Failed to create Trace View window\n");
		return FALSE; // failed to create
	}

	// Create output window
	CString strOutputWnd;
	bNameValid = strOutputWnd.LoadString(IDS_OUTPUT_WND);
	ASSERT(bNameValid);
	if (!m_wndOutput.Create(strOutputWnd, this, CRect(0, 0, 100, 100), TRUE, ID_VIEW_OUTPUTWND, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_BOTTOM | CBRS_FLOAT_MULTI))
	{
		TRACE0("Failed to create Output window\n");
		return FALSE; // failed to create
	}

	// Create properties window
	CString strPropertiesWnd;
	bNameValid = strPropertiesWnd.LoadString(IDS_PROPERTIES_WND);
	ASSERT(bNameValid);
	if (!m_wndProperties.Create(strPropertiesWnd, this, CRect(0, 0, 200, 200), TRUE, ID_VIEW_PROPERTIESWND, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_RIGHT | CBRS_FLOAT_MULTI))
	{
		TRACE0("Failed to create Properties window\n");
		return FALSE; // failed to create
	}

	SetDockingWindowIcons(theApp.m_bHiColorIcons);

	/*if (!_testView.Create(_T("TestCC"), this, CRect(0, 0, 200, 200), TRUE, ID_VIEW_CLASSVIEW, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CBRS_LEFT | CBRS_FLOAT_MULTI))
	{
		TRACE0("Failed to create Class View window\n");
		return FALSE; // failed to create
	}*/

	return TRUE;
}

void CMainFrame::SetDockingWindowIcons(BOOL bHiColorIcons)
{
	HICON hFileViewIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_FILE_VIEW_HC : IDI_FILE_VIEW), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndFileView.SetIcon(hFileViewIcon, FALSE);

	HICON hClassViewIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_CLASS_VIEW_HC : IDI_CLASS_VIEW), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndClassView.SetIcon(hClassViewIcon, FALSE);

	HICON hOutputBarIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_OUTPUT_WND_HC : IDI_OUTPUT_WND), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndOutput.SetIcon(hOutputBarIcon, FALSE);

	HICON hPropertiesBarIcon = (HICON) ::LoadImage(::AfxGetResourceHandle(), MAKEINTRESOURCE(bHiColorIcons ? IDI_PROPERTIES_WND_HC : IDI_PROPERTIES_WND), IMAGE_ICON, ::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON), 0);
	m_wndProperties.SetIcon(hPropertiesBarIcon, FALSE);

}

void CMainFrame::ViewSetFocus(CViewPane* view)
{
	if (_activeViewPane != view)
	{
		_activeViewPane = view;
		
		for (ViewPaneList::iterator iter = _viewPaneList.begin(); iter != _viewPaneList.end(); ++iter)
			(*iter)->OnChangeActiveTab(*iter == view);
	}
}

int CMainFrame::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	if (CFrameWndEx::OnCreate(lpCreateStruct) == -1)
		return -1;

	BOOL bNameValid;

	// set the visual manager used to draw all user interface elements
	CMFCVisualManager::SetDefaultManager(RUNTIME_CLASS(CMFCVisualManagerVS2005));

	if (!m_wndMenuBar.Create(this))
	{
		TRACE0("Failed to create menubar\n");
		return -1;      // fail to create
	}

	m_wndMenuBar.SetPaneStyle(m_wndMenuBar.GetPaneStyle() | CBRS_SIZE_DYNAMIC | CBRS_TOOLTIPS | CBRS_FLYBY);

	// prevent the menu bar from taking the focus on activation
	CMFCPopupMenu::SetForceMenuFocus(FALSE);

	if (!m_wndToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC) ||
		!m_wndToolBar.LoadToolBar(theApp.m_bHiColorIcons ? IDR_MAINFRAME : IDR_MAINFRAME_256))
	{
		TRACE0("Failed to create toolbar\n");
		return -1;      // fail to create
	}

	CString strToolBarName;
	bNameValid = strToolBarName.LoadString(IDS_TOOLBAR_STANDARD);
	ASSERT(bNameValid);
	m_wndToolBar.SetWindowText(strToolBarName);

	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);
	m_wndToolBar.EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);

	if (!m_wndStatusBar.Create(this))
	{
		TRACE0("Failed to create status bar\n");
		return -1;      // fail to create
	}
	m_wndStatusBar.SetIndicators(indicators, sizeof(indicators)/sizeof(UINT));

	// TODO: Delete these five lines if you don't want the toolbar and menubar to be dockable
	m_wndMenuBar.EnableDocking(CBRS_ALIGN_ANY);
	m_wndToolBar.EnableDocking(CBRS_ALIGN_ANY);
	EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndMenuBar);
	DockPane(&m_wndToolBar);


	// enable Visual Studio 2005 style docking window behavior
	CDockingManager::SetDockingMode(DT_SMART);
	// enable Visual Studio 2005 style docking window auto-hide behavior
	EnableAutoHidePanes(CBRS_ALIGN_ANY);

	// Load menu item image (not placed on any standard toolbars):
	CMFCToolBar::AddToolBarForImageCollection(IDR_MENU_IMAGES, theApp.m_bHiColorIcons ? IDB_MENU_IMAGES_24 : 0);

	// create docking windows
	if (!CreateDockingWindows())
	{
		TRACE0("Failed to create docking windows\n");
		return -1;
	}

	m_wndFileView.EnableDocking(CBRS_ALIGN_ANY);
	m_wndClassView.EnableDocking(CBRS_ALIGN_ANY);
	_traceView.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndFileView);
	CDockablePane* pTabbedBar = NULL;
	m_wndClassView.AttachToTabWnd(&m_wndFileView, DM_SHOW, TRUE, &pTabbedBar);
	_traceView.AttachToTabWnd(&m_wndFileView, DM_SHOW, TRUE, &pTabbedBar);
	m_wndOutput.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndOutput);
	m_wndProperties.EnableDocking(CBRS_ALIGN_ANY);
	DockPane(&m_wndProperties);


	// Enable toolbar and docking window menu replacement
	EnablePaneMenu(TRUE, ID_VIEW_CUSTOMIZE, strCustomize, ID_VIEW_TOOLBAR);

	// enable quick (Alt+drag) toolbar customization
	CMFCToolBar::EnableQuickCustomization();




	{
		if (!_editMapToolBar.CreateEx(this, TBSTYLE_FLAT, WS_CHILD | WS_VISIBLE | CBRS_TOP | CBRS_GRIPPER | CBRS_TOOLTIPS | CBRS_FLYBY | CBRS_SIZE_DYNAMIC, CRect(1, 1, 1, 1), IDR_EDITMAP) || !_editMapToolBar.LoadToolBar(IDR_EDITMAP))
		{
			TRACE0("Failed to create toolbar\n");
			return -1;      // fail to create
		}

		CString toolBarName;
		bNameValid = toolBarName.LoadString(IDS_EDITMAP);
		ASSERT(bNameValid);
		_editMapToolBar.SetWindowText(toolBarName);
		
		CString strCustomize;
		bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
		ASSERT(bNameValid);
		_editMapToolBar.EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);

		_editMapToolBar.EnableDocking(CBRS_ALIGN_ANY);
		DockPane(&_editMapToolBar);
	}

	return 0;
}

void CMainFrame::OnViewCustomize()
{
	CMFCToolBarsCustomizeDialog* pDlgCust = new CMFCToolBarsCustomizeDialog(this, TRUE /* scan menus */);
	pDlgCust->Create();
}

LRESULT CMainFrame::OnToolbarCreateNew(WPARAM wp,LPARAM lp)
{
	LRESULT lres = CFrameWndEx::OnToolbarCreateNew(wp,lp);
	if (lres == 0)
	{
		return 0;
	}

	CMFCToolBar* pUserToolbar = (CMFCToolBar*)lres;
	ASSERT_VALID(pUserToolbar);

	BOOL bNameValid;
	CString strCustomize;
	bNameValid = strCustomize.LoadString(IDS_TOOLBAR_CUSTOMIZE);
	ASSERT(bNameValid);

	pUserToolbar->EnableCustomizeButton(TRUE, ID_VIEW_CUSTOMIZE, strCustomize);
	return lres;
}

void CMainFrame::OnEditMapToolBarCommand(UINT nID)
{
	if (!GetMapDoc())
		return;

	switch (nID)
	{
	case IDTB_EDITMAP_NONE:
		GetMapDoc()->SetSelMode(r3d::ISceneControl::smNone);
		m_wndClassView.SetActive(false);
		break;

	case IDTB_EDITMAP_MOVE:
		GetMapDoc()->SetSelMode(r3d::ISceneControl::smMove);
		m_wndClassView.SetActive(false);
		break;

	case IDTB_EDITMAP_ROT:
		GetMapDoc()->SetSelMode(r3d::ISceneControl::smRotate);
		m_wndClassView.SetActive(false);
		break;

	case IDTB_EDITMAP_SCALE:
		GetMapDoc()->SetSelMode(r3d::ISceneControl::smScale);
		m_wndClassView.SetActive(false);
		break;

	case IDTB_EDITMAP_SHOWBB:
		theApp.GetWorld()->GetEdit()->GetMap()->SetShowBBox(!theApp.GetWorld()->GetEdit()->GetMap()->GetShowBBox());
		break;

	case IDTB_EDITMAP_LINKBB:
		theApp.GetWorld()->GetEdit()->GetScControl()->SetLinkBB(!theApp.GetWorld()->GetEdit()->GetScControl()->GetLinkBB());
		break;

	case ID_AUTO_ROT:
		m_wndClassView.SetAutoRot(!m_wndClassView.GetAutoRot());
		break;

	case ID_AUTO_SCALE:
		m_wndClassView.SetAutoScale(!m_wndClassView.GetAutoScale());
		break;

	case ID_CAM_PERSP:
		theApp.GetWorld()->GetICamera()->ChangeStyle(r3d::game::ICameraManager::csFreeView);
		break;

	case ID_CAM_ISO:
		theApp.GetWorld()->GetICamera()->ChangeStyle(r3d::game::ICameraManager::csIsoView);
		break;

	case ID_CAM_LIGHT:
		theApp.GetWorld()->GetICamera()->ChangeStyle(r3d::game::ICameraManager::csLights);
		break;
	}
}

void CMainFrame::OnEditMapToolBarUpdateCommandUI(CCmdUI* pCmdUI)
{
	//pCmdUI->Enable(_mapDoc ? true : false);
	if (!GetMapDoc())
		return;

	bool check = false;

	switch (pCmdUI->m_nID)
	{
	case IDTB_EDITMAP_NONE:
		check = GetMapDoc()->GetSelMode() == r3d::ISceneControl::smNone;
		break;

	case IDTB_EDITMAP_MOVE:
		check = GetMapDoc()->GetSelMode() == r3d::ISceneControl::smMove;
		break;

	case IDTB_EDITMAP_ROT:
		check = GetMapDoc()->GetSelMode() == r3d::ISceneControl::smRotate;
		break;

	case IDTB_EDITMAP_SCALE:
		check = GetMapDoc()->GetSelMode() == r3d::ISceneControl::smScale;
		break;	

	case IDTB_EDITMAP_SHOWBB:
		check = theApp.GetWorld()->GetEdit()->GetMap()->GetShowBBox();
		break;

	case IDTB_EDITMAP_LINKBB:
		check = theApp.GetWorld()->GetEdit()->GetScControl()->GetLinkBB();
		break;

	case ID_AUTO_ROT:
		check = m_wndClassView.GetAutoRot();
		break;

	case ID_AUTO_SCALE:
		check = m_wndClassView.GetAutoScale();
		break;

	case ID_CAM_PERSP:
		check = theApp.GetWorld()->GetICamera()->GetStyle() == r3d::game::ICameraManager::csFreeView;
		break;

	case ID_CAM_ISO:
		check = theApp.GetWorld()->GetICamera()->GetStyle() == r3d::game::ICameraManager::csIsoView;
		break;

	case ID_CAM_LIGHT:
		check = theApp.GetWorld()->GetICamera()->GetStyle() == r3d::game::ICameraManager::csLights;
		break;
	}

	pCmdUI->SetCheck(check);
}

bool CMainFrame::OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord)
{
	if (_activeViewPane)
		return _activeViewPane->OnMapViewMouseClickEvent(key, state, coord);

	return false;
}

bool CMainFrame::OnMapViewMouseMoveEvent(const lsl::Point& coord)
{
	if (_activeViewPane)
		return _activeViewPane->OnMapViewMouseMoveEvent(coord);

	return false;
}

bool CMainFrame::OnMapViewKeyEvent(unsigned key, lsl::KeyState state)
{
	if (_activeViewPane)
		return _activeViewPane->OnMapViewKeyEvent(key, state);

	return false;
}

CMapEditorView* CMainFrame::GetActiveMapView()
{
	return _activeMapView;
}

void CMainFrame::SetActiveMapView(CMapEditorView* value)
{
	if (_activeMapView != value)
	{
		_activeMapView = value;

		m_wndClassView.SetMapDoc(GetMapDoc());
		m_wndFileView.SetMapDoc(GetMapDoc());
		_traceView.SetMapDoc(GetMapDoc());
	}
}

#ifdef _DEBUG
void CMainFrame::AssertValid() const
{
	CFrameWndEx::AssertValid();
}

void CMainFrame::Dump(CDumpContext& dc) const
{
	CFrameWndEx::Dump(dc);
}
#endif