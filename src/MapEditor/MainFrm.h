#pragma once

#include "FileView.h"
#include "ClassView.h"
#include "TraceView.h"
#include "OutputWnd.h"
#include "PropertiesWnd.h"
#include "MapEditorView.h"

//Главное окно
//Выступает также посредником между отдельными видами
class CMainFrame : public CFrameWndEx
{
	DECLARE_DYNCREATE(CMainFrame)
	DECLARE_MESSAGE_MAP()

	friend class CViewPane;

	typedef CFrameWndEx _MyBase;
private:
	typedef lsl::List<CViewPane*> ViewPaneList;
private:
	CMapEditorView* _activeMapView;
	ViewPaneList _viewPaneList;
	CViewPane* _activeViewPane;
private:
	CMapEditorDoc* GetMapDoc();
protected:
	CMFCMenuBar       m_wndMenuBar;
	CMFCToolBar       m_wndToolBar;
	CMFCStatusBar     m_wndStatusBar;
	CFileView         m_wndFileView;
	CClassView        m_wndClassView;
	COutputWnd        m_wndOutput;
	CPropertiesWnd    m_wndProperties;

	CMFCToolBar       _editMapToolBar;
	CTraceView        _traceView;

	CDockablePane        _testView;

	CMainFrame();

	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);
	BOOL CreateDockingWindows();
	void SetDockingWindowIcons(BOOL bHiColorIcons);
	//Вызваются соответствующими видами при установке фокуса
	void ViewSetFocus(CViewPane* view);

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnViewCustomize();
	afx_msg LRESULT OnToolbarCreateNew(WPARAM wp, LPARAM lp);

	afx_msg void OnEditMapToolBarCommand(UINT nID);
	afx_msg void OnEditMapToolBarUpdateCommandUI(CCmdUI* pCmdUI);
public:
	virtual ~CMainFrame();

	bool OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord);
	bool OnMapViewMouseMoveEvent(const lsl::Point& coord);
	bool OnMapViewKeyEvent(unsigned key, lsl::KeyState state);
	
	//Устанавливает активный документ для редактирования
	CMapEditorView* GetActiveMapView();
	void SetActiveMapView(CMapEditorView* value);	

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};


