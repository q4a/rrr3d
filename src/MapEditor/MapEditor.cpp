#include "stdafx.h"
#include "afxwinappex.h"

#include "MapEditor.h"

#include "MapEditorDoc.h"
#include "MapEditorView.h"

class CAboutDlg : public CDialog
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

BEGIN_MESSAGE_MAP(CAboutDlg, CDialog)
END_MESSAGE_MAP()

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

BEGIN_MESSAGE_MAP(CMapEditorApp, CWinAppEx)
	ON_COMMAND(ID_APP_ABOUT, &CMapEditorApp::OnAppAbout)
	// Standard file based document commands
	ON_COMMAND(ID_FILE_NEW, &CWinAppEx::OnFileNew)
	ON_COMMAND(ID_FILE_OPEN, &CWinAppEx::OnFileOpen)
END_MESSAGE_MAP()

#define CATCH_R3D_STDERROR(expression) catch (const lsl::Error& err){AfxMessageBox(StdStrToCString(err.what()), MB_OK | MB_ICONERROR);expression} \
	             	                   catch(const std::exception& err){lsl::appLog << "stdError: " << err.what() << '\n';AfxMessageBox(StdStrToCString(err.what()), MB_OK | MB_ICONERROR);expression} \
									   catch (...){lsl::appLog << "undefError" << '\n';AfxMessageBox(_T("undefError"), MB_OK | MB_ICONERROR);expression}

CMapEditorApp theApp;




CAboutDlg::CAboutDlg() : CDialog(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialog::DoDataExchange(pDX);
}




CMapEditorApp::CMapEditorApp(): _activeView(0), _world(0), _enableRender(true)
{
	m_bHiColorIcons = TRUE;	
}

void CMapEditorApp::InitR3D(CView* view)
{
	r3d::IView::Desc desc;
	desc.fullscreen = false;
	desc.handle = view->GetSafeHwnd();
	desc.resolution = r3d::Point(800, 600);	

	if (!_world)
	{
		_world = r3d::CreateWorld(desc, false);
		_world->RunWorldEdit();
	}
}

void CMapEditorApp::FreeR3D()
{
	if (_world)
	{
		_world->ExitWorldEdit();
		r3d::ReleaseWorld(_world);
		_world = 0;
	}
}

int CMapEditorApp::InitApp()
{
	// InitCommonControlsEx() is required on Windows XP if an application
	// manifest specifies use of ComCtl32.dll version 6 or later to enable
	// visual styles.  Otherwise, any window creation will fail.
	INITCOMMONCONTROLSEX InitCtrls;
	InitCtrls.dwSize = sizeof(InitCtrls);
	// Set this to include all the common control classes you want to use
	// in your application.
	InitCtrls.dwICC = ICC_WIN95_CLASSES;
	InitCommonControlsEx(&InitCtrls);

	CWinAppEx::InitInstance();

	// Standard initialization
	// If you are not using these features and wish to reduce the size
	// of your final executable, you should remove from the following
	// the specific initialization routines you do not need
	// Change the registry key under which our settings are stored
	// TODO: You should modify this string to be something appropriate
	// such as the name of your company or organization
	SetRegistryKey(_T("Rock3d Map Editor"));
	LoadStdProfileSettings(4);  // Load standard INI file options (including MRU)

	InitContextMenuManager();

	InitKeyboardManager();

	InitTooltipManager();
	CMFCToolTipInfo ttParams;
	ttParams.m_bVislManagerTheme = TRUE;
	theApp.GetTooltipManager()->SetTooltipParams(AFX_TOOLTIP_TYPE_ALL,
		RUNTIME_CLASS(CMFCToolTipCtrl), &ttParams);

	// Register the application's document templates.  Document templates
	//  serve as the connection between documents, frame windows and views
	CSingleDocTemplate* pDocTemplate;
	pDocTemplate = new CSingleDocTemplate(
		IDR_MAINFRAME,
		RUNTIME_CLASS(CMapEditorDoc),
		RUNTIME_CLASS(CMainFrame),       // main SDI frame window
		RUNTIME_CLASS(CMapEditorView));
	if (!pDocTemplate)
		return FALSE;
	AddDocTemplate(pDocTemplate);


	// Enable DDE Execute open
	EnableShellOpen();
	RegisterShellFileTypes(TRUE);

	// Parse command line for standard shell commands, DDE, file open
	CCommandLineInfo cmdInfo;
	ParseCommandLine(cmdInfo);


	// Dispatch commands specified on the command line.  Will return FALSE if
	// app was launched with /RegServer, /Register, /Unregserver or /Unregister.
	if (!ProcessShellCommand(cmdInfo))
		return FALSE;

	// The one and only window has been initialized, so show and update it
	m_pMainWnd->ShowWindow(SW_SHOW);
	m_pMainWnd->UpdateWindow();
	// call DragAcceptFiles only if there's a suffix
	//  In an SDI app, this should occur after ProcessShellCommand
	// Enable drag/drop open
	m_pMainWnd->DragAcceptFiles();

	return TRUE;
}

BOOL CMapEditorApp::OnIdle(LONG lCount)
{
	bool res = false;
	if (_world && (_enableRender || GetMainWnd()->IsTopParentActive()))
	{
		_world->MainProgress();

		res = true;
	}

	return _MyBase::OnIdle(lCount) || res;
}

BOOL CMapEditorApp::InitInstance()
{
	setlocale(LC_ALL, "Russian");
	lsl::appLog.Clear();
	lsl::appLog.Append("Init...");

	try
	{
		return InitApp();
	}
	CATCH_R3D_STDERROR(return 0;)
}

int CMapEditorApp::ExitInstance()
{
	FreeR3D();

	lsl::appLog.Append("Exit");

	return _MyBase::ExitInstance();
}

int CMapEditorApp::Run()
{
	try
	{
		return _MyBase::Run();
	}
	CATCH_R3D_STDERROR(return _MyBase::Run();)
}

void CMapEditorApp::PreLoadState()
{
	BOOL bNameValid;
	CString strName;
	bNameValid = strName.LoadString(IDS_EDIT_MENU);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EDIT);
	bNameValid = strName.LoadString(IDS_EXPLORER);
	ASSERT(bNameValid);
	GetContextMenuManager()->AddMenu(strName, IDR_POPUP_EXPLORER);
}

void CMapEditorApp::LoadCustomState()
{
}

void CMapEditorApp::SaveCustomState()
{
}

void CMapEditorApp::RegR3DView(CView* view)
{
	if (_activeView != view)
	{
		FreeR3D();
		_activeView = view;
		InitR3D(_activeView);
	}
}

void CMapEditorApp::UnRegR3DView(CView* view)
{
	if (_activeView == view)
	{
		FreeR3D();
		_activeView = 0;
	}
}

void CMapEditorApp::OnAppAbout()
{
	CAboutDlg aboutDlg;
	aboutDlg.DoModal();
}

bool CMapEditorApp::GetEnableRender() const
{
	return _enableRender;
}

void CMapEditorApp::SetEnableRender(bool value)
{
	if (_enableRender != value)
		_enableRender= value;
}

CMainFrame* CMapEditorApp::GetMainFrame()
{
	return static_cast<CMainFrame*>(m_pMainWnd);
}

r3d::IWorld* CMapEditorApp::GetWorld()
{
	return _world;
}