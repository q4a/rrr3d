
// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently,
// but are changed infrequently

#pragma once

#ifndef _SECURE_ATL
#define _SECURE_ATL 1
#endif

#ifndef VC_EXTRALEAN
#define VC_EXTRALEAN            // Exclude rarely-used stuff from Windows headers
#endif

#include "targetver.h"

#define _ATL_CSTRING_EXPLICIT_CONSTRUCTORS      // some CString constructors will be explicit

// turns off MFC's hiding of some common and often safely ignored warning messages
#define _AFX_ALL_WARNINGS

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions





#ifndef _AFX_NO_OLE_SUPPORT
#include <afxdtctl.h>           // MFC support for Internet Explorer 4 Common Controls
#endif
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>             // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT

#include <afxcontrolbars.h>     // MFC support for ribbons and control bars




#undef min
#undef max

#pragma push_macro("new")
#pragma push_macro("malloc")
#pragma push_macro("free")

#include "Rock3dGame.h"

#pragma pop_macro("new")
#pragma pop_macro("malloc")
#pragma pop_macro("free")

#ifndef NOMINMAX
#define min(a, b) ((a > b) ? b : a)
#define max(a, b) ((a > b) ? a : b)
#endif




#ifdef _UNICODE
#if defined _M_IX86
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
#endif




inline CString CharToCString(const char* value)
{
	return CString(value);
}

inline CString StdStrToCString(const std::string& value)
{
	return CharToCString(value.c_str());
}

inline std::string CStringToStdStr(const CString& value)
{
	CStringA str(value.GetString());
	return std::string(str.GetString());
}

inline bool StrEq(const TCHAR* str1, const TCHAR* str2)
{
	return StrCmp(str1, str2) == 0;
}




//func - функтор типа bool()(HTREEITEM), res == true - закончить обход
template<class _Func> HTREEITEM ForEachTreeCtrlItem(CTreeCtrl& tree, HTREEITEM item, _Func func)
{
	LSL_ASSERT(item);

	HTREEITEM child = tree.GetChildItem(item);
	while (child)
	{
		HTREEITEM res = ForEachTreeCtrlItem(tree, child, func);
		if (res)
			return res;

		child = tree.GetNextSiblingItem(child);		
	}

	return func(item) ? item : 0;
}

template<class _Func> HTREEITEM ForEachTreeCtrlItem(CTreeCtrl& tree, _Func func)
{
	HTREEITEM item = tree.GetRootItem();
	while (item)
	{
		HTREEITEM res = ForEachTreeCtrlItem(tree, item, func);
		if (res)
			return res;

		item = tree.GetNextSiblingItem(item);		
	}

	return 0;
}