#pragma once

#include <fstream>
//Схема с одним единственным видом
//Все операции с видом идут через App::GetWorld
//Все операции с данными(т.е. с документом) идут через документ
class CMapEditorView final : public CView
{
	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CMapEditorView)

	typedef CView _MyBase;
public:	
	std::string _username;
	bool _tmpIsClear{};
	bool _init_startup_map{};
protected:
	CMapEditorView();

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	void OnActivateFrame(UINT nState, CFrameWnd* pFrameWnd) override;
	void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint) override;
	
	void OnMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord, bool shift, bool ctrl);
	void OnMouseMoveEvent(const lsl::Point& coord, bool shift, bool ctrl);
	void OnKeyEvent(unsigned key, lsl::KeyState state);

	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnMButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);

	afx_msg void OnSize(UINT nType, int cx, int cy);

	r3d::IView* GetR3DView();
public:
	~CMapEditorView() override;

	void OnDraw(CDC* pDC) override;
	BOOL PreCreateWindow(CREATESTRUCT& cs) override;
	void RegistryAction();

	CMapEditorDoc* get_document() const;

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif	
};

#ifndef _DEBUG 
inline CMapEditorDoc* CMapEditorView::get_document() const
   { return reinterpret_cast<CMapEditorDoc*>(m_pDocument); }
#endif