#pragma once

#include "ViewTree.h"
#include "MapEditorDoc.h"

class CClassView : public CViewPane
{
	DECLARE_MESSAGE_MAP()

	typedef CViewPane _MyBase;
private:
	struct ItemData
	{
		ItemData(const r3d::IRecordNodeRef& mNode, const r3d::IMapObjRecRef& mRecord): node(mNode), record(mRecord)
		{
			LSL_ASSERT(!(node && record));
		}

		r3d::IRecordNodeRef node;
		r3d::IMapObjRecRef record;
	};

	class DelDataFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CClassView* _view;
	public:
		DelDataFunc(CClassView* view): _view(view) {}
		
		bool operator()(HTREEITEM item) const
		{
			ItemData* data = _view->GetItemData(item);
			if (data)
				delete data;
			_view->SetItemData(item, 0);

			return false;
		}
	};

	class MapDocEvent: public MapDocObserver
	{
	private:
		CClassView* _view;
	public:
		MapDocEvent(CClassView* view);

		virtual void ReleaseDoc();
	};
private:
	CMapEditorDoc* _mapDoc;
	MapDocEvent* _mapDocEvent;
	r3d::IMapObjRef _newMapObj;
	bool _autoRot;
	bool _autoScale;

	//Функции обновление вида
	void ReleaseItem(HTREEITEM item);
	void ReleaseItems();
	void InsertItem(const r3d::IRecordNodeRef& node, HTREEITEM parent);
	void DelItem(HTREEITEM node);	
	void ClearList();
	void UpdateList();

	//Функции работы с моделью + обновление вида
	void SelectItem(const D3DXVECTOR3& mapObjPos = D3DXVECTOR3(0, 0, 0));
	void DeselectItem();

	r3d::IDataBase* GetDB();
	ItemData* GetItemData(HTREEITEM item);
	void SetItemData(HTREEITEM item, ItemData* value);
	ItemData* GetSelItemData();
protected:
	CViewToolBar m_wndToolBar;
	CViewTree m_wndClassView;
	CImageList m_ClassViewImages;
	UINT m_nCurrSort;

	void FillClassView();

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnClassAddMemberFunction();
	afx_msg void OnClassAddMemberVariable();
	afx_msg void OnClassDefinition();
	afx_msg void OnClassProperties();
	afx_msg void OnNewFolder();
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);	
	afx_msg void OnSort(UINT id);
	afx_msg void OnUpdateSort(CCmdUI* pCmdUI);	

	afx_msg void OnViewSelChanged(NMHDR* arg, LRESULT* res);
	virtual void OnChangeActiveTab(bool active);

	bool OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord);
	bool OnMapViewKeyEvent(unsigned key, lsl::KeyState state);
public:
	CClassView();
	virtual ~CClassView();

	void AdjustLayout();
	void OnChangeVisualStyle();

	CMapEditorDoc* GetMapDoc();
	void SetMapDoc(CMapEditorDoc* value);

	bool GetAutoRot() const;
	void SetAutoRot(bool value);

	bool GetAutoScale() const;
	void SetAutoScale(bool value);
};

