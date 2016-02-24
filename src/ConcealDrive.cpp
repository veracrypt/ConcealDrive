/*
 Copyright (c) 2013-2016 IDRIX. All rights reserved.

 Governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/


// ConcealDrive.cpp : main source file for ConcealDrive.exe
//

#include "stdafx.h"

#include <atlframe.h>
#include <atlctrls.h>
#include <atldlgs.h>

#include "resource.h"

#include "MainDlg.h"

CAppModule _Module;

int ScreenDPI = USER_DEFAULT_SCREEN_DPI;
double DPIScaleFactorX = 1;
double DPIScaleFactorY = 1;
double DlgAspectRatio = 1;

// This function currently serves the following purposes:
// - Determines scaling factors for current screen DPI and GUI aspect ratio.
// - Determines how Windows skews the GUI aspect ratio (which happens when the user has a non-default DPI).
// The determined values must be used when performing some GUI operations and calculations.
BOOL CALLBACK AuxiliaryDlgProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		{
			HDC hDC = GetDC (hwndDlg);

			if (hDC)
			{
				ScreenDPI = GetDeviceCaps (hDC, LOGPIXELSY);
				ReleaseDC (hwndDlg, hDC); 
			}

			DPIScaleFactorX = 1;
			DPIScaleFactorY = 1;
			DlgAspectRatio = 1;

			if (ScreenDPI != USER_DEFAULT_SCREEN_DPI)
			{
				// Windows skews the GUI aspect ratio if the user has a non-default DPI. Hence, working with 
				// actual screen DPI is redundant and leads to incorrect results. What really matters here is
				// how Windows actually renders our GUI. This is determined by comparing the expected and current
				// sizes of a hidden calibration text field.

				RECT trec;

				trec.right = 0;
				trec.bottom = 0;

				GetClientRect (GetDlgItem (hwndDlg, IDC_ASPECT_RATIO_CALIBRATION_BOX), &trec);

				if (trec.right != 0 && trec.bottom != 0)
				{
					// The size of the 282x282 IDC_ASPECT_RATIO_CALIBRATION_BOX rendered at the default DPI (96) is 423x458
					DPIScaleFactorX = (double) trec.right / 423;
					DPIScaleFactorY = (double) trec.bottom / 458;
					DlgAspectRatio = DPIScaleFactorX / DPIScaleFactorY;
				}
			}

			EndDialog (hwndDlg, 0);
			return 1;
		}

	case WM_CLOSE:
		EndDialog (hwndDlg, 0);
		return 1;
	}

	return 0;
}

RTLINITUNICODESTRING RtlInitUnicodeString = NULL;
NTOPENSYMBOLICLINKOBJECT NtOpenSymbolicLinkObject = NULL;
NTQUERYSYMBOLICLINKOBJECT NtQuerySymbolicLinkObject = NULL;
NTCLOSE NtClose= NULL;


int WINAPI _tWinMain(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPTSTR /*lpstrCmdLine*/, int /*nCmdShow*/)
{
	HRESULT hRes = ::CoInitialize(NULL);
// If you are running on NT 4.0 or higher you can use the following call instead to 
// make the EXE free threaded. This means that calls come in on a random RPC thread.
//	HRESULT hRes = ::CoInitializeEx(NULL, COINIT_MULTITHREADED);
	ATLASSERT(SUCCEEDED(hRes));

	// this resolves ATL window thunking problem when Microsoft Layer for Unicode (MSLU) is used
	::DefWindowProc(NULL, 0, 0, 0L);

	AtlInitCommonControls(ICC_BAR_CLASSES);	// add flags to support other controls

	hRes = _Module.Init(NULL, hInstance);
	ATLASSERT(SUCCEEDED(hRes));

   HMODULE _hModule = LoadLibrary(_T("ntdll.dll"));

   RtlInitUnicodeString = (RTLINITUNICODESTRING)GetProcAddress(_hModule, "RtlInitUnicodeString");
   NtOpenSymbolicLinkObject = (NTOPENSYMBOLICLINKOBJECT)GetProcAddress(_hModule, "NtOpenSymbolicLinkObject");
   NtQuerySymbolicLinkObject = (NTQUERYSYMBOLICLINKOBJECT)GetProcAddress(_hModule, "NtQuerySymbolicLinkObject");
   NtClose = (NTCLOSE) GetProcAddress(_hModule, "NtClose");

	// DPI and GUI aspect ratio
	DialogBoxParamW (hInstance, MAKEINTRESOURCEW (IDD_DPI), NULL,
		(DLGPROC) AuxiliaryDlgProc, (LPARAM) 1);

	int nRet = 0;
	// BLOCK: Run application
	{
		CMainDlg dlgMain;
		nRet = dlgMain.DoModal();
	}

	_Module.Term();
	::CoUninitialize();

	return nRet;
}
