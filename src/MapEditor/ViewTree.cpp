#include "stdafx.h"
#include "ViewTree.h"

#include "MapEditor.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#undef THIS_FILE
static char THIS_FILE[] = __FILE__;
#endif

BEGIN_MESSAGE_MAP(CViewTree, CTreeCtrl)
	ON_WM_KEYDOWN()	
END_MESSAGE_MAP()

BEGIN_MESSAGE_MAP(CViewPane, CDockablePane)
	ON_WM_SETFOCUS()
END_MESSAGE_MAP()




CViewTree::CViewTree()
{
}

CViewTree::~CViewTree()
{
}

BOOL CViewTree::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	BOOL bRes = CTreeCtrl::OnNotify(wParam, lParam, pResult);

	NMHDR* pNMHDR = (NMHDR*)lParam;
	ASSERT(pNMHDR != NULL);

	if (pNMHDR && pNMHDR->code == TTN_SHOW && GetToolTips() != NULL)
	{
		GetToolTips()->SetWindowPos(&wndTop, -1, -1, -1, -1, SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOSIZE);
	}

	return bRes;
}

void CViewTree::OnKeyDown(UINT nChar, UINT nRepCnt, UINT nFlags)
{
	CTreeCtrl::OnKeyDown(nChar, nRepCnt, nFlags);

	if (GetParent())
	{
		NMKEY nmh;
		nmh.hdr.code = NM_KEYDOWN;
		nmh.hdr.idFrom = GetDlgCtrlID();
		nmh.hdr.hwndFrom = m_hWnd;
		//
		nmh.nVKey = nChar;
		nmh.uFlags = nFlags;

		GetParent()->SendMessage(WM_NOTIFY, (WPARAM)m_hWnd, (LPARAM)&nmh);
	}
}




CViewPane::CViewPane(): _activeTab(false)
{
}

BOOL CViewPane::OnNotify(WPARAM wParam, LPARAM lParam, LRESULT* pResult)
{
	_MyBase::OnNotify(wParam, lParam, pResult);

	NMHDR* message = (NMHDR*)lParam;

	if (message->code == NM_SETFOCUS && theApp.GetMainFrame())
		theApp.GetMainFrame()->ViewSetFocus(this);

	return TRUE;
}

void CViewPane::OnSetFocus(CWnd* pOldWnd)
{
	_MyBase::OnSetFocus(pOldWnd);

	if (theApp.GetMainFrame())
		theApp.GetMainFrame()->ViewSetFocus(this);
}

void CViewPane::OnChangeActiveTab(bool active)
{
	_activeTab = active;
}

bool CViewPane::IsActiveTab() const
{
	return _activeTab;
}

void CViewPane::SetActive(bool active)
{
	if (active == _activeTab)
		return;

	if (active)
		SetFocus();
	else
	{
		::SetFocus(0);
		theApp.GetMainFrame()->ViewSetFocus(0);
	}
}

LRESULT CViewPane::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
	
	switch (message)
	{

	case WM_NOTIFY:
	{
		NMHDR* mes = (NMHDR*)lParam;
		UINT code = mes->code;
		bool sf = code == NM_SETFOCUS;
		
		int m = 0;
		++m;
		break;
	}

	}


	/*char strBuf[256];
	CFile file(_T("C:\\1.txt"), CFile::modeReadWrite);
	file.SeekToEnd();
	sprintf_s(strBuf, "%d  ", message);
	file.Write(strBuf, strlen(strBuf));
	file.Close();*/

	//message != 0x363 && message != 0x4e && message != 0x20 && message != 0x210 && message != 0x21 && message != 0x400 && message != 0x133 && message != 0x19

	return _MyBase::WindowProc(message, wParam, lParam);
}