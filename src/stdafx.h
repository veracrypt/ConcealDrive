/*
 Copyright (c) 2013-2016 IDRIX. All rights reserved.

 Governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/


// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//  are changed infrequently
//

#pragma once

// Change these values to use different versions
#define WINVER		0x0500
#define _WIN32_WINNT	0x0501
#define _WIN32_IE	0x0501
#define _RICHEDIT_VER	0x0500

#include <atlbase.h>
#include <atlapp.h>

extern CAppModule _Module;

#include <atlwin.h>
#include <atlctrls.h>
#include <atlctrlx.h>
#include <atlmisc.h>
#include <vector>
#include <map>
#include <string>
#include <sstream>
#include <Ntsecapi.h>
#include <strsafe.h>

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS                          ((NTSTATUS)0x00000000L) // ntsubauth
#endif


using namespace std;

#ifndef USER_DEFAULT_SCREEN_DPI
#define USER_DEFAULT_SCREEN_DPI 96
#endif

typedef struct _OBJECT_ATTRIBUTES {
    ULONG Length;
    HANDLE RootDirectory;
    PUNICODE_STRING ObjectName;
    ULONG Attributes;
    PVOID SecurityDescriptor;        // Points to type SECURITY_DESCRIPTOR
    PVOID SecurityQualityOfService;  // Points to type SECURITY_QUALITY_OF_SERVICE
} OBJECT_ATTRIBUTES, *POBJECT_ATTRIBUTES;


#define InitializeObjectAttributes( p, n, a, r, s ) { \
    (p)->Length = sizeof( OBJECT_ATTRIBUTES );          \
    (p)->RootDirectory = r;                             \
    (p)->Attributes = a;                                \
    (p)->ObjectName = n;                                \
    (p)->SecurityDescriptor = s;                        \
    (p)->SecurityQualityOfService = NULL;               \
    }

#define OBJ_CASE_INSENSITIVE 0x00000040
#define OBJ_KERNEL_HANDLE 0x00000200

typedef VOID (WINAPI *RTLINITUNICODESTRING)(PUNICODE_STRING DestinationString,
    PCWSTR SourceString);

typedef NTSTATUS (WINAPI *NTOPENSYMBOLICLINKOBJECT)(
_Out_  PHANDLE LinkHandle,
_In_   ACCESS_MASK DesiredAccess,
_In_   POBJECT_ATTRIBUTES ObjectAttributes
);
 
typedef NTSTATUS (WINAPI *NTQUERYSYMBOLICLINKOBJECT)(
_In_       HANDLE LinkHandle,
_Inout_    PUNICODE_STRING LinkTarget,
_Out_opt_  PULONG ReturnedLength
);

typedef NTSTATUS(NTAPI *NTCLOSE)(IN HANDLE Handle);

extern RTLINITUNICODESTRING RtlInitUnicodeString;
extern NTOPENSYMBOLICLINKOBJECT NtOpenSymbolicLinkObject;
extern NTQUERYSYMBOLICLINKOBJECT NtQuerySymbolicLinkObject;
extern NTCLOSE NtClose;



#if defined _M_IX86
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='x86' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_IA64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='ia64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#elif defined _M_X64
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")
#else
  #pragma comment(linker, "/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
#endif
