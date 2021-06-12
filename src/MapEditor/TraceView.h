#pragma once

#include "ViewTree.h"
#include "MapEditorDoc.h"

class CTraceView : public CViewPane
{
	DECLARE_MESSAGE_MAP()

	typedef CViewPane _MyBase;
private:
	struct PntItemData
	{
		PntItemData(const r3d::IWayPointRef& mWp): wp(mWp)
		{
		}

		r3d::IWayPointRef wp;
	};
	class DelDataFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CTraceView* _view;
	public:
		DelDataFunc(CTraceView* view): _view(view) {}
		
		bool operator()(HTREEITEM item) const
		{
			PntItemData* data = _view->GetPointItemData(item);
			if (data)
				delete data;
			_view->SetPointItemData(item, 0);

			return false;
		}
	};
	class FindItemByPointFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CTraceView* _view;
		r3d::IWayPointRef _point;
	public:
		FindItemByPointFunc(CTraceView* view, const r3d::IWayPointRef& point): _view(view), _point(point) {}
		
		bool operator()(HTREEITEM item) const
		{
			PntItemData* data = _view->GetPointItemData(item);

			return data && data->wp && data->wp->Equal(_point.Pnt());
		}
	};

	struct PathItemData
	{
		PathItemData(const r3d::IWayPathRef& mPath, const r3d::IWayNodeRef& mNode): path(mPath), node(mNode) {
			LSL_ASSERT(!(path && node));
		}

		r3d::IWayPathRef path;
		r3d::IWayNodeRef node;
	};
	class DelPathDataFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CTraceView* _view;
	public:
		DelPathDataFunc(CTraceView* view): _view(view) {}
		
		bool operator()(HTREEITEM item) const
		{
			PathItemData* data = _view->GetPathItemData(item);
			if (data)
				delete data;
			_view->SetPathItemData(item, 0);

			return false;
		}
	};
	class FindItemByPathFunc: public std::unary_function<HTREEITEM, bool>
	{
	private:
		CTraceView* _view;
		r3d::IWayPathRef _path;
	public:
		FindItemByPathFunc(CTraceView* view, const r3d::IWayPathRef& path): _view(view), _path(path) {}
		
		bool operator()(HTREEITEM item) const
		{
			PathItemData* data = _view->GetPathItemData(item);

			return data && data->path && data->path->Equal(_path.Pnt());
		}
	};

	class PathControlEvent: public r3d::ITrace::ControlEvent
	{
	private:
		CTraceView* _view;
	public:
		PathControlEvent(CTraceView* view): _view(view) {}

		virtual void OnAddAndSelPoint(const r3d::IWayPointRef& ref)
		{
			if (_view->_mapDoc && _view->_pathView)
			{
				_view->InsertPointItem(ref);
				_view->SelectPoint(ref);
			}
		}

		virtual void OnPathesUpdate()
		{
			if (_view->_mapDoc && _view->_pathView)
				_view->UpdatePathList();
		}
	};

	class MapDocEvent: public MapDocObserver
	{
	private:
		CTraceView* _view;
	public:
		MapDocEvent(CTraceView* view): _view(view) {}

		virtual void OnUpdate()
		{
			_view->UpdateList();
		}

		virtual void ReleaseDoc()
		{
			_view->ClearList();
		}
	};
private:
	CMapEditorDoc* _mapDoc;	
	MapDocEvent* _mapDocEvent;

	//Функции обновления вида
	//

	//Список точек
	void ReleasePointItem(HTREEITEM item);
	void ReleasePointItems();
	void InsertPointItem(r3d::IWayPointRef waypoint);
	void RemovePointItem(HTREEITEM item);
	HTREEITEM FindPointItem(const r3d::IWayPointRef& point);
	//
	void ClearPointList();
	void UpdatePointList();
	
	//Список путей
	void ReleasePathItem(HTREEITEM item);
	void ReleasePathItems();
	//	
	void InsertNodeItem(const r3d::IWayNodeRef& node, HTREEITEM parent);
	void InsertPathItem(r3d::IWayPathRef path);
	void RemovePathItem(HTREEITEM item);
	HTREEITEM FindPathItem(const r3d::IWayPathRef& path);
	//
	void ClearPathList();
	void UpdatePathList();
	
	//Общие функции
	void ClearList();
	void UpdateList();

	//Редактирование модели + обновление вида
	//Поскольку данные изолированны в данном классе, то редактирование идет в обход документа для упрощения
	void AddPoint();
	void DelSelPoint();
	void SelectPoint(HTREEITEM item);
	void SelectPoint(const r3d::IWayPointRef& point);

	void AddPath();
	void DelSelPath();
	void SelectPath(HTREEITEM item);
	void SelectPath(const r3d::IWayPathRef& path);
	//
	void SelectNode(HTREEITEM item);	

	r3d::ITrace* GetTrace();
	//
	PntItemData* GetPointItemData(HTREEITEM item);
	PntItemData* GetSelPointItemData();
	void SetPointItemData(HTREEITEM item, PntItemData* value);
	//
	PathItemData* GetPathItemData(HTREEITEM item);
	void SetPathItemData(HTREEITEM item, PathItemData* value);
protected:
	CSplitterWnd _splitter;
	CFrameWnd* _flatFrame;

	CViewTree _pointView;
	CViewTree _pathView;

	CImageList _images;
	CViewToolBar _toolBar;

	void AdjustLayout();
	bool IsActiveTreeView(CViewTree* view);

	afx_msg int OnCreate(LPCREATESTRUCT lpCreateStruct);
	afx_msg void OnDestroy();
	afx_msg void OnSize(UINT nType, int cx, int cy);
	afx_msg void OnPaint();
	afx_msg void OnSetFocus(CWnd* pOldWnd);

	afx_msg void OnCommandNew();
	afx_msg void OnCommandDel();
	afx_msg void OnEditMapToolBarCommand();

	afx_msg void OnPointViewSelChanged(NMHDR* arg, LRESULT* res);
	afx_msg void OnPathViewSelChanged(NMHDR* arg, LRESULT* res);
	virtual void OnChangeActiveTab(bool active);

	bool OnMapViewMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord);
public:
	CTraceView();
	virtual ~CTraceView();

	CMapEditorDoc* GetMapDoc();
	void SetMapDoc(CMapEditorDoc* value);
};