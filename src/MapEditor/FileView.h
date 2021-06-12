#pragma once

#include "ViewTree.h"
#include "MapEditorDoc.h"

class CFileView: public CViewPane
{
	DECLARE_MESSAGE_MAP()

	typedef CViewPane _MyBase;
private:
	struct ItemData
	{
		ItemData(const r3d::IMapObjRef& mMapObj, int mCatIndex): mapObj(mMapObj), catIndex(mCatIndex)
		{
			LSL_ASSERT(!(mapObj && catIndex > -1));
		}

		r3d::IMapObjRef mapObj;
		int catIndex;
	};

	class DelDataFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CFileView* _view;
	public:
		DelDataFunc(CFileView* view): _view(view) {}
		
		bool operator()(HTREEITEM item) const
		{
			ItemData* data = _view->GetItemData(item);
			if (data)
				delete data;
			_view->SetItemData(item, 0);

			return false;
		}
	};
	class FindItemByMapObjFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CFileView* _view;
		r3d::IMapObjRef _mapObj;
	public:
		FindItemByMapObjFunc(CFileView* view, const r3d::IMapObjRef& mapObj): _view(view), _mapObj(mapObj) {}
		
		bool operator()(HTREEITEM item) const
		{
			ItemData* data = _view->GetItemData(item);

			return data && data->mapObj && data->mapObj->Equal(_mapObj.Pnt());
		}
	};

	class MapDocEvent: public MapDocObserver
	{
	private:
		CFileView* _view;
	public:
		MapDocEvent(CFileView* view);

		virtual void OnAddMapObj(const r3d::IMapObjRef& ref);
		virtual void OnDelMapObj(const r3d::IMapObjRef& ref);
		virtual void OnSelectMapObj(const r3d::IMapObjRef& ref);
		virtual void OnUpdate();
		virtual void ReleaseDoc();
	};
private:
	CMapEditorDoc* _mapDoc;
	MapDocEvent* _mapDocEvent;

	//Функции обновления вида
	//Вспомогательные
	void ReleaseItem(HTREEITEM item);
	void ReleaseItems();
	void InsertItem(const r3d::IMapObjRef& mapObj, HTREEITEM parent);
	void InsertItemCat(const r3d::IMapObjRef& mapObj);
	void RemoveItem(const r3d::IMapObjRef& mapObj);
	void InsertCategory(unsigned i);
	HTREEITEM FindCatItem(unsigned i);
	HTREEITEM FindMapObjItem(const r3d::IMapObjRef& mapObj);
	//Общие
	void ClearList();
	void UpdateList();
	//Выделения
	void SelectItem(HTREEITEM item);
	void SelectItem(const r3d::IMapObjRef& mapObj);
	void DeselectItem();

	//Операции доступа
	ItemData* GetItemData(HTREEITEM item);
	void SetItemData(HTREEITEM item, ItemData* value);
	ItemData* GetSelItemData();
protected:
	CViewTree m_wndFileView;
	CImageList m_FileViewImages;
	CViewToolBar m_wndToolBar;

	void FillFileView();

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnContextMenu(CWnd* pWnd, CPoint point);
	afx_msg void OnProperties();
	afx_msg void OnFileOpen();
	afx_msg void OnFileOpenWith();
	afx_msg void OnDummyCompile();
	afx_msg void OnEditCut();
	afx_msg void OnEditCopy();
	afx_msg void OnEditClear();
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);

	afx_msg void OnViewSelChanged(NMHDR* arg, LRESULT* res);
	afx_msg void OnViewKeyDown(NMHDR* arg, LRESULT* res);
	virtual void OnChangeActiveTab(bool active);
public:
	CFileView();
	virtual ~CFileView();

	void AdjustLayout();
	void OnChangeVisualStyle();

	CMapEditorDoc* GetMapDoc();
	void SetMapDoc(CMapEditorDoc* value);
};

