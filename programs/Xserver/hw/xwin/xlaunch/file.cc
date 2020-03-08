/*
 * Copyright (c) 2005-2007 Colin Harrison All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name(s) of the above copyright
 * holders shall not be used in advertising or otherwise to promote the sale,
 * use or other dealings in this Software without prior written authorization.
 *
 * Authors:	Colin Harrison
 */

/*------------------------------------------
   File.cc -- File Functions
  ------------------------------------------*/

#include <windows.h>
#include <stdio.h>
#include <string>
#include "resources/resources.h"

LPCWSTR FilePath (HWND hwnd, bool PuTTY)
     {
     OPENFILENAMEW ofn;
     std::wstring szPath;
     wchar_t szFile[MAX_PATH] = L"", szTitle[512], szFilterPuTTY[512], szFilterSSH[512];
     HINSTANCE hInst = GetModuleHandle(NULL);
     LoadStringW(hInst, IDS_FILEPATH_TITLE, szTitle, sizeof(szTitle)/sizeof(wchar_t));
     LoadStringW(hInst, IDS_PUTTYPATH_FILTER, szFilterPuTTY, sizeof(szFilterPuTTY)/sizeof(wchar_t));
     LoadStringW(hInst, IDS_SSHPATH_FILTER, szFilterSSH, sizeof(szFilterSSH)/sizeof(wchar_t));
     for (unsigned i=0; szFilterPuTTY[i]; i++)
        if (szFilterPuTTY[i] == L'%')
            szFilterPuTTY[i] = L'\0';
     for (unsigned i=0; szFilterSSH[i]; i++)
        if (szFilterSSH[i] == L'%')
            szFilterSSH[i] = L'\0';

     ZeroMemory(&ofn, sizeof(OPENFILENAMEW));

     ofn.lStructSize       = sizeof(OPENFILENAMEW);
     ofn.hwndOwner         = hwnd;
     ofn.lpstrFile         = szFile;
     ofn.lpstrFile[0]      = L'\0';
     ofn.nMaxFile          = sizeof(szFile);
     if (PuTTY)
        ofn.lpstrFilter    = szFilterPuTTY;
     else
        ofn.lpstrFilter    = szFilterSSH;
     ofn.nFilterIndex      = 1;
     ofn.lpstrFileTitle    = NULL;
     ofn.nMaxFileTitle     = 0;
     ofn.lpstrInitialDir   = NULL;
     ofn.lpstrTitle        = szTitle;
     ofn.Flags             = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_READONLY | OFN_HIDEREADONLY;
     ofn.lpstrDefExt       = L"exe";

     szPath[0]             = '\0';
     if (GetOpenFileNameW (&ofn))
	{
	szPath = szFile;
	szPath = szPath.substr (0, ofn.nFileOffset);
	for (int i = 0; i < szPath.length(); ++i)
		if (szPath[i] == L'\\')
			{
			szPath.insert(i, 1, L'\\');
			++i; // Skip inserted char
			}
	}
     return szPath.c_str();
     }

BOOL FileRead (LPCWSTR lpstrFileName)
     {
     FILE  *file;

     if (NULL == (file = _wfopen (lpstrFileName, L"r")))
	return FALSE;
     fclose (file);
     return TRUE;
     }
