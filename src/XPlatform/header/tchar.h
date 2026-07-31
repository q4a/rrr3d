#ifndef XPLATFORM_TCHAR_H
#define XPLATFORM_TCHAR_H

/*
 * Stands in for Microsoft's <tchar.h>.
 *
 * TCHAR exists so one source tree can build as either ANSI or UTF-16. This
 * project only ever builds ANSI -- _UNICODE is referenced in exactly one place,
 * an MFC block in MapEditor's stdafx.h, and every string literal reaching a
 * Win32 call is narrow. So TCHAR is char and _T() is the identity, which is
 * what the Windows build already resolves them to.
 *
 * Only the members the tree actually uses: _T (172 sites), TCHAR (14),
 * LPCTSTR (6), LPTSTR, _tprintf.
 */

#include "xplatform.h"

#include <stdio.h>
#include <string.h>

typedef char            TCHAR;
typedef char            _TCHAR;
typedef char*           LPTSTR;
typedef const char*     LPCTSTR;
typedef char*           PTSTR;
typedef const char*     PCTSTR;

#define _T(x)   x
#define TEXT(x) x
#define __TEXT(x) x

#define _tprintf    printf
#define _ftprintf   fprintf
#define _stprintf_s sprintf_s
#define _tcslen     strlen
#define _tcscpy     strcpy
#define _tcscmp     strcmp
#define _tcsicmp    strcasecmp
#define _tcschr     strchr
#define _tcsrchr    strrchr
#define _tcsstr     strstr
#define _tfopen     fopen

#endif
