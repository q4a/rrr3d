#pragma once

#ifndef __AFXWIN_H__
	#error "include 'stdafx.h' before including this file for PCH"
#endif

#include "resource.h"       // main symbols
#include "MainFrm.h"

class CMapEditorApp : public CWinAppEx
{
	DECLARE_MESSAGE_MAP()

	typedef CWinAppEx _MyBase;
private:
	CView* _activeView;
	r3d::IWorld* _world;
	bool _enableRender;

	void InitR3D(CView* view);
	void FreeR3D();

	int InitApp();
protected:
	virtual BOOL OnIdle(LONG lCount);
public:
	CMapEditorApp();

	virtual BOOL InitInstance();
	virtual int ExitInstance();
	virtual int Run();

	virtual void PreLoadState();
	virtual void LoadCustomState();
	virtual void SaveCustomState();

	//Регистарция вида
	void RegR3DView(CView* view);
	void UnRegR3DView(CView* view);

	afx_msg void OnAppAbout();

	bool GetEnableRender() const;
	void SetEnableRender(bool value);

	CMainFrame* GetMainFrame();
	r3d::IWorld* GetWorld();

	BOOL  m_bHiColorIcons;
};

extern CMapEditorApp theApp;
