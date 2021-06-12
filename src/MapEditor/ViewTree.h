#pragma once

/////////////////////////////////////////////////////////////////////////////
// CViewTree window

class CViewTree : public CTreeCtrl
{
// Construction
public:
	CViewTree();

// Overrides
protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	//¬ текущей версии NM_KEY не поддерживаетс€, поэтому формируем это уведомление самосто€тельно
	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);

// Implementation
public:
	virtual ~CViewTree();

protected:
	DECLARE_MESSAGE_MAP()
};

//ѕересылает команды к родительскому окну
class CViewToolBar : public CMFCToolBar
{
	virtual void OnUpdateCmdUI(CFrameWnd* /*pTarget*/, BOOL bDisableIfNoHndler)
	{
		CMFCToolBar::OnUpdateCmdUI((CFrameWnd*) GetOwner(), bDisableIfNoHndler);
	}

	virtual BOOL AllowShowOnList() const { return FALSE; }
};

//Ѕазовый класс дл€ вьеверов карты с управлением фокусом
class CViewPane: public CDockablePane
{
	DECLARE_MESSAGE_MAP()

	friend class CMainFrame;
	typedef CDockablePane _MyBase;
private:
	bool _activeTab;
protected:
	virtual BOOL OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult);
	afx_msg void OnSetFocus(CWnd* pOldWnd);

	virtual void OnChangeActiveTab(bool active);	
	virtual bool OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord) {return false;}
	virtual bool OnMapViewMouseMoveEvent(const lsl::Point& coord) {return false;}
	virtual bool OnMapViewKeyEvent(unsigned key, lsl::KeyState state) {return false;}
public:
	CViewPane();

	bool IsActiveTab() const;
	void SetActive(bool active);

	virtual LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam);
};