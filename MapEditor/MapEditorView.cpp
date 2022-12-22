#include "stdafx.h"
#include "MapEditor.h"

#include "MapEditorDoc.h"
#include "MapEditorView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

IMPLEMENT_DYNCREATE(CMapEditorView, CView)

BEGIN_MESSAGE_MAP(CMapEditorView, CView)
	ON_WM_CREATE()
	ON_WM_DESTROY()
	ON_WM_KEYDOWN()
	ON_WM_KEYUP()
	ON_WM_LBUTTONDOWN()
	ON_WM_LBUTTONUP()
	ON_WM_MBUTTONDOWN()
	ON_WM_MBUTTONUP()
	ON_WM_RBUTTONDOWN()
	ON_WM_RBUTTONUP()
	ON_WM_MOUSEMOVE()

	ON_WM_SIZE()
END_MESSAGE_MAP()


/**
 * \brief 
 */
CMapEditorView::CMapEditorView()
{
}

CMapEditorView::~CMapEditorView()
= default;

int CMapEditorView::OnCreate(LPCREATESTRUCT lpCreateStruct)
{
	unsigned res = _MyBase::OnCreate(lpCreateStruct);
	_init_startup_map = false;
	theApp.RegR3DView(this);

	//Имя пользователя windows
	DWORD size = 1024;
	char buf[1024];
	GetUserNameA(buf, &size);
	_username = buf;

	return res;
}

void CMapEditorView::OnDestroy()
{
	theApp.GetMainFrame()->SetActiveMapView(nullptr);

	theApp.UnRegR3DView(this);

	_MyBase::OnDestroy();
}

void CMapEditorView::OnActivateFrame(UINT nState, CFrameWnd* pFrameWnd)
{
	theApp.GetMainFrame()->SetActiveMapView(this);

	_MyBase::OnActivateFrame(nState, pFrameWnd);

	if (_init_startup_map == false)
	{
		_tmpIsClear = false;
		//ссылки на файлы
		std::ifstream file1(StdStrToCString(R"(C:\Users\)" + _username +
			R"(\AppData\Local\XRayDevStudios\MapEditor\startup\actualMap.tmp)"));
		std::ifstream file2(StdStrToCString(R"(C:\Users\)" + _username +
			R"(\AppData\Local\XRayDevStudios\MapEditor\startup\actualTitle.tmp)"));

		//Документ который загружаем при первом запуске.
		std::string startup_patch;
		if (file1.is_open())
		{
			while (!file1.eof())
			{
				getline(file1, startup_patch);
			}
			file1.close();
		}

		//Заголовок трассы 
		std::string startup_title = "novamap";

		//Если файл не пустой, то загружаем трассу, которая открыта утилитой.
		if (file2.is_open())
		{
			while (!file2.eof())
			{
				getline(file2, startup_title);
			}
			file2.close();
		}

		if (!startup_patch.empty())
		{
			get_document()->OnOpenDocument(StdStrToCString(startup_patch));
			file2 >> startup_title;
		}
		get_document()->SetTitle(StdStrToCString(startup_title));
		_init_startup_map = true;
	}
}

void CMapEditorView::OnUpdate(CView* pSender, LPARAM lHint, CObject* pHint)
{
	_MyBase::OnUpdate(pSender, lHint, pHint);
}

void CMapEditorView::OnMouseClickEvent(lsl::MouseKey key, lsl::KeyState state, const lsl::Point& coord, bool shift,
                                       bool ctrl)
{
	if (!get_document())
		return;

	bool res = GetR3DView()->OnMouseClickEvent(key, state, coord, shift, ctrl);
	res = res || theApp.GetMainFrame()->OnMapViewMouseClickEvent(key, state, coord);

	if (!res && key == lsl::mkLeft && state == lsl::ksDown)
	{
		get_document()->SelectMapObj(get_document()->PickMapObj(coord));
	}

	r3d::IMapObjRef ref = get_document()->GetSelMapObj();
	if (ref)
	{
		if (get_document()->IsMapObjCreated())
		{
			RegistryAction();
		}
		else
		{
			if (theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos() != theApp.GetWorld()->GetEdit()->
				GetScControl()->GetSelNode()->GetLastPos())
			{
				theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetLastPos(
					theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos());
				RegistryAction();
			}

			if (theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetScale() != theApp.GetWorld()->GetEdit()->
				GetScControl()->GetSelNode()->GetLastScale())
			{
				theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetLastScale(
					theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetScale());
				RegistryAction();
			}

			if (theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetRot() != theApp.GetWorld()->GetEdit()->
				GetScControl()->GetSelNode()->GetLastRot())
			{
				theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetLastRot(
					theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetRot());
				RegistryAction();
			}
		}
	}
}

void CMapEditorView::OnMouseMoveEvent(const lsl::Point& coord, bool shift, bool ctrl)
{
	GetR3DView()->OnMouseMoveEvent(coord, shift, ctrl);
	theApp.GetMainFrame()->OnMapViewMouseMoveEvent(coord);
}

void CMapEditorView::OnKeyEvent(unsigned key, lsl::KeyState state)
{
	bool res = GetR3DView()->OnKeyEvent(key, state, false);
	res = res || theApp.GetMainFrame()->OnMapViewKeyEvent(key, state);

	if (!res && key == VK_DELETE && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			get_document()->SelectMapObj(nullptr);
			get_document()->DelMapObj(ref);
		}
	}

	//Backspace - откат на предыдущий hIndex:
	if (key == 0x08 && state == lsl::ksDown)
	{
		if (HINDEX > 0)
		{
			HINDEX -= 1;
			std::string autobackPatch = "C:\\Users\\" + _username + lsl::StrFmt(
				"\\AppData\\Local\\XRayDevStudios\\MapEditor\\autosave\\autoback%d.tmp", HINDEX);
			get_document()->OnOpenDocument(StdStrToCString(autobackPatch));
		}
	}

	//TAB - refresh document.
	if (key == VK_TAB && state == lsl::ksDown)
	{
		get_document()->OnOpenDocument(get_document()->GetPathName());
	}

	//Q - режим перемещения объектов.
	if (key == 0x51 && state == lsl::ksDown)
	{
		//D3DVIEWPORT9 viewPort = { 0, 0, 800, 600, 0, 1 };
		//_engine = new r3d::graph::Engine* (window, resolution, fullScreen, _multisampling);
		//_engine->GetDriver().GetDevice()->GetViewport(&viewPort);
		//get_document()->SetSelMode(r3d::ISceneControl::smMove);
	}

	//E - режим масштабирования объектов.
	if (key == 0x45 && state == lsl::ksDown)
	{
		get_document()->SetSelMode(r3d::ISceneControl::smScale);
	}

	//Space - режим выделения объектов.
	if (key == VK_SPACE && state == lsl::ksDown)
	{
		get_document()->SetSelMode(r3d::ISceneControl::smNone);
	}

	//X - ротация по оси X
	if (key == 0x58 && state == lsl::ksDown)
	{
		get_document()->SetSelMode(r3d::ISceneControl::smRotX);
	}

	//Y - ротация по оси Y
	if (key == 0x59 && state == lsl::ksDown)
	{
		get_document()->SetSelMode(r3d::ISceneControl::smRotY);
	}

	//Z - ротация по оси Z
	if (key == 0x5A && state == lsl::ksDown)
	{
		get_document()->SetSelMode(r3d::ISceneControl::smRotZ);
	}

	//F - FlyToObject 
	if (key == 0x46 && state == lsl::ksDown)
	{
		//GetDocument()->FixedClear();
		get_document()->SetSelMode(r3d::ISceneControl::smCamLink);
	}

	//UP - плавный сдвиг объекта по оси Y
	if (key == 0x26 && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(nodePos + D3DXVECTOR3(0, 0.002f, 0));
		}
	}

	//DOWN - плавный сдвиг объекта по оси Y
	if (key == 0x28 && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(nodePos - D3DXVECTOR3(0, 0.002f, 0));
		}
	}

	//LEFT - плавный сдвиг объекта по оси X
	if (key == 0x25 && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(nodePos + D3DXVECTOR3(0.002f, 0, 0));
		}
	}

	//RIGHT - плавный сдвиг объекта по оси X
	if (key == 0x27 && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(nodePos - D3DXVECTOR3(0.002f, 0, 0));
		}
	}

	//+ плавное поднятие объекта по оси Z
	if ((key == 0xBB || key == 0x6B) && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(nodePos + D3DXVECTOR3(0, 0, 0.001f));
		}
	}

	//- плавное опускание объекта по оси Z
	if ((key == 0xBD || key == 0x6D) && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(nodePos - D3DXVECTOR3(0, 0, 0.001f));
		}
	}

	//J - JumpCopy
	if (key == 0x4A && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			get_document()->SetSelMode(r3d::ISceneControl::smMove);
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			float nodeHeight = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetAABB().GetSizes().z;
			D3DXVECTOR3 clonePos = nodePos + D3DXVECTOR3(0, 0, nodeHeight);
			D3DXVECTOR3 cloneVec = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetUp();
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->OnShiftAction(clonePos, cloneVec);
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(clonePos);
			this->RegistryAction();
		}
	}

	//K - KickCopy
	if (key == 0x4B && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			get_document()->SetSelMode(r3d::ISceneControl::smMove);
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			float GizmoWidth = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetAABB().GetSizes().x;
			D3DXVECTOR3 cloneVec = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetDir();
			D3DXVECTOR3 clonePos = nodePos + (cloneVec * GizmoWidth);
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->OnShiftAction(clonePos, cloneVec);
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(clonePos);
			this->RegistryAction();
		}
	}

	//L - LedgeCopy
	if (key == 0x4C && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			get_document()->SetSelMode(r3d::ISceneControl::smMove);
			D3DXVECTOR3 nodePos = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetPos();
			float GizmoWidth = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetAABB().GetSizes().y;
			D3DXVECTOR3 cloneVec = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetRight();
			D3DXVECTOR3 clonePos = nodePos + (cloneVec * GizmoWidth);
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->OnShiftAction(clonePos, cloneVec);
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetPos(clonePos);
			this->RegistryAction();
		}
	}

	//G - Gizmo show/hide
	if (key == 0x47 && state == lsl::ksDown)
	{
		if (theApp.GetWorld()->GetEdit()->GetMap()->GetShowBBox() == false)
			theApp.GetWorld()->GetEdit()->GetMap()->SetShowBBox(true);
		else if (theApp.GetWorld()->GetEdit()->GetMap()->GetShowBBox() == true)
			theApp.GetWorld()->GetEdit()->GetMap()->SetShowBBox(false);
	}

	//Caps Lock - Привязка перемещения в напревлениях XY к размерам Gizmo
	if (key == VK_CAPITAL && state == lsl::ksDown)
	{
		theApp.GetWorld()->GetEdit()->GetScControl()->SetLinkBB(
			!theApp.GetWorld()->GetEdit()->GetScControl()->GetLinkBB());
	}

	//R - random rotate по оси Z:
	if (key == 0x52 && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			D3DXQUATERNION nodeRot = theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->GetRot();
			D3DXQUATERNION rotZ;
			D3DXQuaternionRotationAxis(&rotZ, &ZVector, RandomRange(0.0f, 360.0f));

			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetRot(nodeRot * rotZ);
			theApp.GetWorld()->GetEdit()->GetScControl()->GetSelNode()->SetLastRot(nodeRot);
			RegistryAction();
		}
	}

	//T - trashbox. Удаление объекта:
	if (key == 0x54 && state == lsl::ksDown)
	{
		r3d::IMapObjRef ref = get_document()->GetSelMapObj();
		if (ref)
		{
			get_document()->ClearSelection();
			get_document()->DelMapObj(ref);
			get_document()->SetSelMode(r3d::ISceneControl::smMove);
			this->RegistryAction();
		}
	}

	if (key == VK_SHIFT && state == lsl::ksDown)
	{
		if (theApp.GetWorld()->GetEdit()->GetScControl()->GetCopyEvent())
		{
			RegistryAction();
			theApp.GetWorld()->GetEdit()->GetScControl()->SetCopyEvent(false);
		}
	}
}

void CMapEditorView::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	OnKeyEvent(nChar, lsl::ksDown);
}

void CMapEditorView::OnKeyUp(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	OnKeyEvent(nChar, lsl::ksUp);
}

void CMapEditorView::OnLButtonDown(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkLeft, lsl::ksDown, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                  (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnLButtonUp(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkLeft, lsl::ksUp, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                  (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnRButtonDown(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkRight, lsl::ksDown, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                  (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnRButtonUp(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkRight, lsl::ksUp, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                  (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnMButtonDown(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkMiddle, lsl::ksDown, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                  (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnMButtonUp(UINT nFlags, CPoint point)
{
	OnMouseClickEvent(lsl::mkMiddle, lsl::ksUp, lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                  (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnMouseMove(UINT nFlags, CPoint point)
{
	OnMouseMoveEvent(lsl::Point(point.x, point.y), (nFlags & MK_SHIFT) ? true : false,
	                 (nFlags & MK_CONTROL) ? true : false);
}

void CMapEditorView::OnContextMenu(CWnd* pWnd, CPoint point)
{
	theApp.GetContextMenuManager()->ShowPopupMenu(IDR_POPUP_EDIT, point.x, point.y, this, TRUE);
}

void CMapEditorView::OnSize(UINT nType, int cx, int cy)
{
	if (GetR3DView() && cx > 100 && cy > 100)
	{
		r3d::IView::Desc desc;
		desc.fullscreen = false;
		desc.resolution = lsl::Point(cx, cy);
		desc.handle = GetSafeHwnd();
		GetR3DView()->Reset(desc);

		//GetR3DView()->SetCameraAspect(static_cast<float>(cx) / cy);
	}
}

r3d::IView* CMapEditorView::GetR3DView()
{
	return theApp.GetWorld()->GetView();
}

BOOL CMapEditorView::PreCreateWindow(CREATESTRUCT& cs)
{
	return CView::PreCreateWindow(cs);
}

void CMapEditorView::OnDraw(CDC* /*pDC*/)
{
	CMapEditorDoc* pDoc = get_document();
	ASSERT_VALID(pDoc);
	if (!pDoc)
		return;

	if (_init_startup_map == true && _tmpIsClear == false)
	{
		//видаляємо тимчасові файли, бо вони будуть тільки заважати... 
		system(R"(del /s /q /f C:\Users\%username%\AppData\Local\XRayDevStudios\MapEditor\startup)");
		_tmpIsClear = true;
	}
}

void CMapEditorView::RegistryAction()
{
	if (!get_document())
		return;

	//Проверяем не изменился ли документ, если изменился, то сбрасываем историю действий, через historyIndex.
	if (get_document()->GetLastTitle() != CStringToStdStr(get_document()->GetTitle()))
	{
		HINDEX = 0;
		get_document()->SetLastTitle(CStringToStdStr(get_document()->GetTitle()));
	}

	//Место хранения истории действий.
	std::string autobackPatch = R"(C:\Users\)" + _username + lsl::StrFmt(
		R"(\AppData\Local\XRayDevStudios\MapEditor\autosave\autoback%d.tmp)", HINDEX);

	get_document()->OnSaveDocument(StdStrToCString(autobackPatch));
	HINDEX += 1;

	//сохраняем информацию о пути к файлу в .tmp файл:
	std::string fileNew = R"(C:\Users\)" + _username +
		R"(\AppData\Local\XRayDevStudios\MapEditor\autosave\lastPath.tmp)";
	std::string filePath = CStringToStdStr(get_document()->GetPathName());

	std::ofstream out(fileNew.c_str());
	if (out.is_open())
	{
		out << filePath.c_str() << std::endl;
	}
	out.close();
}

#ifdef _DEBUG
void CMapEditorView::AssertValid() const
{
	CView::AssertValid();
}

void CMapEditorView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

CMapEditorDoc* CMapEditorView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(CMapEditorDoc)));
	return (CMapEditorDoc*)m_pDocument;
}
#endif
