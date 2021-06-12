#pragma once

//Схема с одним единственным видом
//Все операции с видом идут через App::GetWorld
//Все операции с данными(т.е. с документом) идут через документ
class CMapEditorView : public CView
{
	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CMapEditorView)

	typedef CView _MyBase;
protected:
	CMapEditorView();

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	virtual void OnActivateFrame(UINT nState, CFrameWnd* pFrameWnd);
	virtual void OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint);
	
	void OnMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord, bool shift, bool ctrl);
	void OnMouseMoveEvent(const lsl::Point& coord, bool shift, bool ctrl);
	void OnKeyEvent(unsigned key, lsl::KeyState state);

	afx_msg void OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags);
	afx_msg void OnLButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnLButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnRButtonDown(UINT nFlags, CPoint point);
	afx_msg void OnRButtonUp(UINT nFlags, CPoint point);
	afx_msg void OnMouseMove(UINT nFlags, CPoint point);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);

	afx_msg void OnSize(UINT nType, int cx, int cy);

	r3d::IView* GetR3DView();
public:
	virtual ~CMapEditorView();

	virtual void OnDraw(CDC* pDC);
	virtual BOOL PreCreateWindow(CREATESTRUCT& cs);

	CMapEditorDoc* GetDocument() const;

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif	
};

#ifndef _DEBUG 
inline CMapEditorDoc* CMapEditorView::GetDocument() const
   { return reinterpret_cast<CMapEditorDoc*>(m_pDocument); }
#endif