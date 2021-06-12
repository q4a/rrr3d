#pragma once

class MapDocObserver: public lsl::Object
{
public:
	virtual void OnAddMapObj(const r3d::IMapObjRef& ref) {}
	virtual void OnDelMapObj(const r3d::IMapObjRef& ref) {}
	virtual void OnSelectMapObj(const r3d::IMapObjRef& ref) {}
	virtual void OnUpdate() {}

	virtual void ReleaseDoc() {}
};

class CMapEditorDoc : public CDocument
{
	DECLARE_MESSAGE_MAP()
	DECLARE_DYNCREATE(CMapEditorDoc)

	typedef CDocument _MyBase;
private:
	typedef lsl::Container<MapDocObserver*> ObserverList;

	class MapContrEvent: public r3d::IMap::ControlEvent
	{
	private:
		CMapEditorDoc* _doc;
	public:
		MapContrEvent(CMapEditorDoc* doc): _doc(doc) {}

		virtual void OnAddAndSelMapObj(const r3d::IMapObjRef& mapObj)
		{
			//Объект может быть уничтожен!
			CMapEditorDoc* doc = _doc;

			for (ObserverList::Position pos = doc->_observerList.First(); MapDocObserver** iter = doc->_observerList.Current(pos); doc->_observerList.Next(pos))	
			{
				(*iter)->OnAddMapObj(mapObj);
				(*iter)->OnSelectMapObj(mapObj);
			}
		}
	};
private:
	ObserverList _observerList;
	r3d::IMapObjRef _selMapObj;
	r3d::IWayPointRef _selWayPoint;

	void ClearSelection();

	void ReleaseDoc();
	void UpdateDoc();

	r3d::IEdit* GetEdit();
	r3d::IMap* GetMap();
	r3d::ITrace* GetTrace();
	r3d::ISceneControl* GetScControl();
protected:
	CMapEditorDoc();

	virtual BOOL OnNewDocument();
	virtual BOOL OnSaveDocument(LPCTSTR lpszPathName);
	virtual BOOL OnOpenDocument(LPCTSTR lpszPathName);
	virtual void OnCloseDocument();	
public:
	virtual ~CMapEditorDoc();

	virtual void Serialize(CArchive& ar);

	void RegObserver(MapDocObserver* observer);
	void UnregObserver(MapDocObserver* observer);

	r3d::IMapObjRef AddMapObj(const r3d::IMapObjRecRef& record);
	void DelMapObj(r3d::IMapObjRef& ref);
	r3d::IMapObjRef PickMapObj(const lsl::Point& coord);
	//
	std::string GetCatName(unsigned i);
	unsigned GetCatCount();
	//
	r3d::IMapObjRef GetFirst(unsigned cat);
	void GetNext(unsigned cat, r3d::IMapObjRef& ref);
	//
	r3d::IMapObjRef GetSelMapObj();
	void SelectMapObj(const r3d::IMapObjRef& value);
	//
	r3d::IWayPointRef GetSelWayPoint();
	void SelectWayPoint(const r3d::IWayPointRef& point, const r3d::ITrace::ControlEventRef& mEvent);
	//
	r3d::ISceneControl::SelMode GetSelMode();
	void SetSelMode(r3d::ISceneControl::SelMode value);

#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif
};


