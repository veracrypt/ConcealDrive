/*
 Derived from source code of TrueCrypt 7.1a, which is
 Copyright (c) 2008-2012 TrueCrypt Developers Association and which is governed
 by the TrueCrypt License 3.0.

 Modifications and additions to the original source code (contained in this file) 
 and all other portions of this file are Copyright (c) 2013-2016 IDRIX
 and are governed by the Apache License 2.0 the full text of which is
 contained in the file License.txt included in VeraCrypt binary and source
 code distribution packages.
*/

#include "StdAfx.h"
#include "resource.h"
#include "MainDlg.h"


#define TC_MAX_VOLUME_SECTOR_SIZE				4096
#define TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE		(2 * TC_MAX_VOLUME_SECTOR_SIZE)
#define TC_NTFS_CONCEAL_CONSTANT	0xFF

#define EXCL_ACCESS_MAX_AUTO_RETRIES 500
#define EXCL_ACCESS_AUTO_RETRY_DELAY 10

#define MAX_HOST_DRIVE_NUMBER 64
#define MAX_HOST_PARTITION_NUMBER 32

#if BYTE_ORDER == LITTLE_ENDIAN
#	define BE16(x) MirrorBytes16(x)
#	define BE32(x) MirrorBytes32(x)
#	define BE64(x) MirrorBytes64(x)
#else
#	define BE16(x) (x)
#	define BE32(x) (x)
#	define BE64(x) (x)
#endif

extern int ScreenDPI;
extern double DPIScaleFactorX;
extern double DPIScaleFactorY;
extern double DlgAspectRatio;

inline ULONGLONG MirrorBytes64 (ULONGLONG x)
{
	ULONGLONG n = (unsigned __int8) x;
	n <<= 8; n |= (unsigned __int8) (x >> 8);
	n <<= 8; n |= (unsigned __int8) (x >> 16);
	n <<= 8; n |= (unsigned __int8) (x >> 24);
	n <<= 8; n |= (unsigned __int8) (x >> 32);
	n <<= 8; n |= (unsigned __int8) (x >> 40);
	n <<= 8; n |= (unsigned __int8) (x >> 48);
	return (n << 8) | (unsigned __int8) (x >> 56);
}

void GetSizeString (unsigned __int64 size, wchar_t *str, size_t cbStr)
{
	if (size > 1024I64*1024*1024*1024*1024)
		StringCbPrintfW (str, cbStr, L"%.2f PiB",(double)size/ (double) (1024.0*1024*1024*1024*1024));
	else if (size > 1024I64*1024*1024*1024)
		StringCbPrintfW (str, cbStr, L"%.2f TiB",(double)size/(double) (1024.0*1024*1024*1024));
	else if (size > 1024I64*1024*1024)
		StringCbPrintfW (str, cbStr, L"%.2f GiB",(double)size/(double) (1024.0*1024*1024));
	else if (size > 1024I64*1024)
		StringCbPrintfW (str, cbStr, L"%.2f MiB",(double)size/(double) (1024.0*1024));
	else if (size >= 1024I64)
		StringCbPrintfW (str, cbStr, L"%.2f KiB", (double) size/ (double) 1024.0);
	else
		StringCbPrintfW (str, cbStr, L"%I64d Bytes", size);
}

LRESULT ListItemAdd (HWND list, int index, const wchar_t *string)
{
	LVITEM li;
	memset (&li, 0, sizeof(li));

	li.mask = LVIF_TEXT;
	li.pszText = (wchar_t*) string;
	li.iItem = index; 
	li.iSubItem = 0;
	return ListView_InsertItem (list, &li);
}


LRESULT ListSubItemSet (HWND list, int index, int subIndex, const wchar_t *string)
{
	LVITEM li;
	memset (&li, 0, sizeof(li));

	li.mask = LVIF_TEXT;
	li.pszText = (wchar_t*) string;
	li.iItem = index; 
	li.iSubItem = subIndex;
	return ListView_SetItem (list, &li);
}

void Error (LPCTSTR szMsg, HWND hWnd)
{
   MessageBox (hWnd, szMsg, L"Error", MB_ICONERROR);
}

DWORD handleWin32Error (HWND hwndDlg)
{
	PWSTR lpMsgBuf;
	DWORD dwError = GetLastError ();	
	wchar_t szErrorValue[32];
	wchar_t* pszDesc;

	if (dwError == 0)
		return dwError;

	FormatMessageW (
		FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			      NULL,
			      dwError,
			      MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),	/* Default language */
			      (PWSTR) &lpMsgBuf,
			      0,
			      NULL
	    );

	if (lpMsgBuf)
		pszDesc = (wchar_t*) lpMsgBuf;
	else
	{
		StringCchPrintfW (szErrorValue, ARRAYSIZE (szErrorValue), L"Error 0x%.8X", dwError);
		pszDesc = szErrorValue;
	}

   MessageBoxW (hwndDlg, pszDesc, L"System Error", MB_ICONHAND);
	if (lpMsgBuf) LocalFree (lpMsgBuf);

	SetLastError (dwError);		// Preserve the original error code

	return dwError;
}

HANDLE OpenPartitionVolume (HWND hwndDlg, LPCWSTR devName)
{
	HANDLE dev = INVALID_HANDLE_VALUE;
	int retryCount = 0;

	// Exclusive access
	// Note that when exclusive access is denied, it is worth retrying (usually succeeds after a few tries).
	while (dev == INVALID_HANDLE_VALUE && retryCount++ < EXCL_ACCESS_MAX_AUTO_RETRIES)
	{
		dev = CreateFileW (devName, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_FLAG_WRITE_THROUGH, NULL);

		if (retryCount > 1)
			Sleep (EXCL_ACCESS_AUTO_RETRY_DELAY);
	}

	if (dev == INVALID_HANDLE_VALUE)
	{
      handleWin32Error (hwndDlg);

      Error (L"Error: Cannot access the volume and/or obtain information about the volume.\n\nMake sure that the selected volume exists, that it is not being used by the system or applications, that you have read/write permission for the volume, and that it is not write-protected.", hwndDlg);
	   return INVALID_HANDLE_VALUE;
	}

	return dev;
}

bool FakeDosNameForDevice (DWORD counter, const wchar_t *lpszDiskFile , wchar_t *lpszDosDevice , size_t cbDosDevice, wchar_t *lpszCFDevice , size_t cbCFDevice, BOOL bNameOnly)
{
	BOOL bDosLinkCreated = TRUE;
	StringCbPrintfW (lpszDosDevice, cbDosDevice,L"concealdrivevc%lu%lu", GetCurrentProcessId (), counter);

	if (bNameOnly == FALSE)
		bDosLinkCreated = DefineDosDevice (DDD_RAW_TARGET_PATH, lpszDosDevice, lpszDiskFile);

	if (bDosLinkCreated == FALSE)
		return false;
	else
		StringCbPrintfW (lpszCFDevice, cbCFDevice,L"\\\\.\\%s", lpszDosDevice);

	return true;
}

// Easy-to-undo modification applied to conceal the NTFS filesystem (to prevent Windows and apps from 
// interfering with it until the volume has been fully encrypted). Note that this function will precisely
// undo any modifications it made to the filesystem automatically if an error occurs when writing (including
// physical drive defects).
static bool ConcealNTFS (HANDLE dev, bool& bHadFileSystemBefore, bool& bHasFilesystemNow)
{
	char buf [TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE];
	DWORD nbrBytesProcessed, nbrBytesProcessed2;
	int i;
	LARGE_INTEGER offset;
	DWORD dwError;

	offset.QuadPart = 0;
 
	if (SetFilePointerEx (dev, offset, NULL, FILE_BEGIN) == 0)
		return false;

	if (ReadFile (dev, buf, TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE, &nbrBytesProcessed, NULL) == 0)
		return false;

   bHadFileSystemBefore = false;
   bHasFilesystemNow = false;

	switch (BE64 (*(ULONGLONG *) buf))
	{
	case 0xEB52904E54465320: // NTFS
	case 0xEB3C904D53444F53: // FAT16
	case 0xEB58904D53444F53: // FAT32
	case 0xEB76904558464154: // exFAT

		bHadFileSystemBefore = TRUE;
		break;
	}

	for (i = 0; i < TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE; i++)
		buf[i] ^= TC_NTFS_CONCEAL_CONSTANT;

	offset.QuadPart = 0;

	if (SetFilePointerEx (dev, offset, NULL, FILE_BEGIN) == 0)
		return false;

	if (WriteFile (dev, buf, TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE, &nbrBytesProcessed, NULL) == 0)
	{
		// One or more of the sectors is/are probably damaged and cause write errors.
		// We must undo the modifications we made.

		dwError = GetLastError();

		for (i = 0; i < TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE; i++)
			buf[i] ^= TC_NTFS_CONCEAL_CONSTANT;

		offset.QuadPart = 0;

		do
		{
			Sleep (1);
		}
		while (SetFilePointerEx (dev, offset, NULL, FILE_BEGIN) == 0
			|| WriteFile (dev, buf, TC_INITIAL_NTFS_CONCEAL_PORTION_SIZE, &nbrBytesProcessed2, NULL) == 0);

		SetLastError (dwError);

		return false;
	}

	switch (BE64 (*(ULONGLONG *) buf))
	{
	case 0xEB52904E54465320: // NTFS
	case 0xEB3C904D53444F53: // FAT16
	case 0xEB58904D53444F53: // FAT32
	case 0xEB76904558464154: // exFAT

      bHasFilesystemNow = TRUE;
		break;
	}

	return true;
}

struct HostDevice
{
	HostDevice ()
		:
		Bootable (false),
		ContainsSystem (false),
		DynamicVolume (false),
		Floppy (false),
		IsPartition (false),
		IsVirtualPartition (false),
		Removable (false),
		Size (0),
		SystemNumber((DWORD) -1)
	{
	}

	~HostDevice () { }

	bool Bootable;
	bool ContainsSystem;
	bool DynamicVolume;
	bool Floppy;
	bool IsPartition;
	bool IsVirtualPartition;
	std::wstring MountPoint;
	std::wstring Name;
	std::wstring Path;
	bool Removable;
	ULONGLONG Size;
	DWORD SystemNumber;

	std::vector <HostDevice> Partitions;
};

struct RawDevicesDlgParam
{
	std::vector <HostDevice> devices;
	wchar_t *pszFileName;
};

// If the user has a non-default screen DPI, some screen coordinates and sizes must
// be converted using this function
int CompensateXDPI (int val)
{
	if (ScreenDPI == USER_DEFAULT_SCREEN_DPI)
		return val;
	else
	{
		double tmpVal = (double) val * DPIScaleFactorX;

		if (tmpVal > 0)
			return (int) floor(tmpVal);
		else
			return (int) ceil(tmpVal);
	}
}


// If the user has a non-default screen DPI, some screen coordinates and sizes must
// be converted using this function
int CompensateYDPI (int val)
{
	if (ScreenDPI == USER_DEFAULT_SCREEN_DPI)
		return val;
	else
	{
		double tmpVal = (double) val * DPIScaleFactorY;

		if (tmpVal > 0)
			return (int) floor(tmpVal);
		else
			return (int) ceil(tmpVal);
	}
}

BOOL GetDriveGeometry (HANDLE hDev, PDISK_GEOMETRY diskGeometry)
{
	BOOL bResult;
	DWORD bytesRead = 0;

	ZeroMemory (diskGeometry, sizeof (DISK_GEOMETRY));

	if (	DeviceIoControl (hDev, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, diskGeometry, sizeof (DISK_GEOMETRY), &bytesRead, NULL)
		&& (bytesRead == sizeof (DISK_GEOMETRY)) 
		&& diskGeometry->BytesPerSector)
	{
		bResult = TRUE;
	}

	return bResult;
}

BOOL GetPhysicalDriveGeometry (int driveNumber, PDISK_GEOMETRY diskGeometry)
{
	HANDLE hDev;
	BOOL bResult = FALSE;
	TCHAR devicePath[MAX_PATH];

	StringCchPrintfW (devicePath, ARRAYSIZE (devicePath), L"\\\\.\\PhysicalDrive%d", driveNumber);

	if ((hDev = CreateFileW (devicePath, 0, 0, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
	{
		DWORD bytesRead = 0;

		ZeroMemory (diskGeometry, sizeof (DISK_GEOMETRY));

		if (	DeviceIoControl (hDev, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, diskGeometry, sizeof (DISK_GEOMETRY), &bytesRead, NULL)
			&& (bytesRead == sizeof (DISK_GEOMETRY)) 
			&& diskGeometry->BytesPerSector)
		{
			bResult = TRUE;
		}

		CloseHandle (hDev);
	}

	return bResult;
}

bool SymbolicLinkToTarget (PWSTR symlinkName, PWSTR targetName, USHORT maxTargetNameLength)
{
	NTSTATUS ntStatus;
	OBJECT_ATTRIBUTES objectAttributes;
	UNICODE_STRING fullFileName;
	HANDLE handle;

	RtlInitUnicodeString (&fullFileName, symlinkName);
	InitializeObjectAttributes (&objectAttributes, &fullFileName, OBJ_KERNEL_HANDLE | OBJ_CASE_INSENSITIVE, NULL, NULL);

	ntStatus = NtOpenSymbolicLinkObject (&handle, GENERIC_READ, &objectAttributes);

	if (STATUS_SUCCESS == ntStatus)
	{
		UNICODE_STRING target;
		target.Buffer = targetName;
		target.Length = 0;
		target.MaximumLength = maxTargetNameLength;
		memset (targetName, 0, maxTargetNameLength);

		ntStatus = NtQuerySymbolicLinkObject (handle, &target, NULL);

		NtClose (handle);
	}

	return STATUS_SUCCESS == ntStatus;
}


// Returns drive letter number assigned to device (-1 if none)
int GetDiskDeviceDriveLetter (PWSTR deviceName)
{
	int i;
	WCHAR link[MAX_PATH];
	WCHAR target[MAX_PATH];
	WCHAR device[MAX_PATH];

	if (!SymbolicLinkToTarget (deviceName, device, sizeof(device)))
		StringCchCopyW (device, MAX_PATH, deviceName);

	for (i = 0; i < 26; i++)
	{
		WCHAR drive[] = { (WCHAR) i + L'A', L':', 0 };

		StringCchCopyW (link, MAX_PATH, L"\\DosDevices\\");
		StringCchCatW (link, MAX_PATH, drive);

		if (	SymbolicLinkToTarget (link, target, sizeof(target))
			&& (wcscmp (device, target) == 0)
			)
		{
			return i;
		}
	}

	return -1;
}

BOOL GetDriveLabel (int driveNo, wchar_t *label, int labelSize)
{
	DWORD fileSystemFlags;
	wchar_t root[] = { L'A' + (wchar_t) driveNo, L':', L'\\', 0 };

	return GetVolumeInformationW (root, label, labelSize / 2, NULL, NULL, &fileSystemFlags, NULL, 0);
}

// Returns 0 if an error occurs or the drive letter (as an upper-case char) of the system partition (e.g. 'C');
wchar_t GetSystemDriveLetter (void)
{
	wchar_t systemDir [MAX_PATH];

	if (GetSystemDirectory (systemDir, ARRAYSIZE (systemDir)))
		return (wchar_t) (towupper (systemDir [0]));
	else
		return 0;
}


bool IsWindowsVista ()
{
   static bool bIsVista = false;
   static bool bIsChecked = false;

   if (!bIsChecked)
   {
      OSVERSIONINFOEXW os;
	   os.dwOSVersionInfoSize = sizeof (OSVERSIONINFOEXW);

      if (GetVersionExW ((LPOSVERSIONINFOW) &os) && os.dwMajorVersion >= 6)
      {
         bIsVista= true;
      }

      bIsChecked = true;
   }

   return bIsVista;
}

typedef struct
{
	PARTITION_INFORMATION partInfo;
	BOOL IsGPT;
	BOOL IsDynamic;
}
DISK_PARTITION_INFO_STRUCT;

BOOL GetDeviceInfo (HANDLE hDev, DISK_PARTITION_INFO_STRUCT *info)
{
	DWORD bytesRead;
   BOOL bResult = FALSE;

   PARTITION_INFORMATION_EX pi;   

   if ((bResult = DeviceIoControl (hDev, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0, &pi, sizeof (PARTITION_INFORMATION_EX), &bytesRead, NULL)))
   {
		memset (&info->partInfo, 0, sizeof (info->partInfo));

		info->partInfo.PartitionLength = pi.PartitionLength;
		info->partInfo.PartitionNumber = pi.PartitionNumber;
		info->partInfo.StartingOffset = pi.StartingOffset;

		if (pi.PartitionStyle == PARTITION_STYLE_MBR)
		{
			info->partInfo.PartitionType = pi.Mbr.PartitionType;
			info->partInfo.BootIndicator = pi.Mbr.BootIndicator;
		}

		info->IsGPT = pi.PartitionStyle == PARTITION_STYLE_GPT;
	}
	else
	{
      bResult = DeviceIoControl (hDev, IOCTL_DISK_GET_PARTITION_INFO, NULL, 0, &info->partInfo, sizeof (PARTITION_INFORMATION), &bytesRead, NULL);			
		info->IsGPT = FALSE;
	}

	if (!bResult)
	{
		GET_LENGTH_INFORMATION lengthInfo;
      bResult = DeviceIoControl (hDev, IOCTL_DISK_GET_LENGTH_INFO, NULL, 0, &lengthInfo, sizeof (GET_LENGTH_INFORMATION), &bytesRead, NULL);

		if (bResult)
		{
			memset (&info->partInfo, 0, sizeof (info->partInfo));
			info->partInfo.PartitionLength = lengthInfo.Length;
		}
	}

	info->IsDynamic = FALSE;

	if (bResult && IsWindowsVista())
	{
#define IOCTL_VOLUME_IS_DYNAMIC CTL_CODE(IOCTL_VOLUME_BASE, 18, METHOD_BUFFERED, FILE_ANY_ACCESS)
      if (!DeviceIoControl (hDev, IOCTL_VOLUME_IS_DYNAMIC, NULL, 0, &info->IsDynamic, sizeof (info->IsDynamic), &bytesRead, NULL))
			info->IsDynamic = FALSE;
	}

   return bResult;
}


void UpdateDeviceInfo (HostDevice& device)
{
   int driveNumber = GetDiskDeviceDriveLetter ((wchar_t *) device.Path.c_str());

   if (driveNumber >= 0)
   {
	   device.MountPoint = (wchar_t) (driveNumber + L'A');
	   device.MountPoint += L":";

	   wchar_t name[64];
	   if (GetDriveLabel (driveNumber, name, sizeof (name)))
		   device.Name = name;

	   if (GetSystemDriveLetter() == L'A' + driveNumber)
		   device.ContainsSystem = true;
   }
}

wstring volumeInfo(WCHAR *volName)
{
   wstringstream strm;
  {
     
    //First some basic volume info
    WCHAR volumeName[MAX_PATH + 1] = { 0 };
    WCHAR fileSystemName[MAX_PATH + 1] = { 0 };
    DWORD serialNumber = 0;
    DWORD maxComponentLen = 0;
    DWORD fileSystemFlags = 0;
    if (GetVolumeInformation(volName, volumeName, ARRAYSIZE(volumeName), &serialNumber, &maxComponentLen, &fileSystemFlags, fileSystemName, ARRAYSIZE(fileSystemName)))
    {
      strm << L"Label: [" << volumeName << L"]\n";
      strm << L"SerNo: " << serialNumber << endl;
      strm << L"FS: [" << fileSystemName << L"]\n";
      //wprintf(L"Label: [%s]  ", volumeName);
      //wprintf(L"SerNo: %lu  ", serialNumber);
      //wprintf(L"FS: [%s]\n", fileSystemName);
  //    wprintf(L"Max Component Length: %lu\n", maxComponentLen);
    }
    else
    {
      TCHAR msg[MAX_PATH + 1];
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, MAX_PATH, NULL);
      strm << L"Last error: " <<msg <<endl;
      //wprintf(L"Last error: %s", msg);
    }
  }
  {
    //The following code finds all folders that are mount points on this volume (empty folder that has another volume mounted-in)
    //This requires administrative privileges so unless you run the app as an admin, the function will simply return nothing
    //It's pretty much useless anyway because the same info can be obtained in the following section where we get mount points for a volume - so reverse lookup is quite possible
    HANDLE mp;
    WCHAR volumeName[MAX_PATH + 1] = { 0 };
    bool success;
    mp = FindFirstVolumeMountPoint(volName, volumeName, MAX_PATH);
    success = mp != INVALID_HANDLE_VALUE;
    if (!success)
    { //This will yield "Access denied" unless we run the app in administrative mode
      TCHAR msg[MAX_PATH + 1];
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, MAX_PATH, NULL);
      strm << L"Evaluate mount points error: " << msg << endl;
    }
    while (success)
    {
      strm << L"Mount point: " << volumeName << endl;
      success = FindNextVolumeMountPoint(mp, volumeName, MAX_PATH) != 0;
    }
    FindVolumeMountPointClose(mp);
  }

  {
    //Now find the mount points for this volume
    DWORD charCount = MAX_PATH;
    WCHAR *mp = NULL, *mps = NULL;
    bool success;

    while (true)
    {
      mps = new WCHAR[charCount];
      success = GetVolumePathNamesForVolumeNameW(volName, mps, charCount, &charCount) != 0;
      if (success || GetLastError() != ERROR_MORE_DATA) 
        break;
      delete [] mps;
      mps = NULL;
    }
    if (success)
    {
      for (mp = mps; mp[0] != '\0'; mp += wcslen(mp))
        strm << L"Mount point: " << mp << endl;
    }
    delete [] mps;
  }

  {
    //And the type of this volume
    switch (GetDriveType(volName))
    {
    case DRIVE_UNKNOWN:     strm << "unknown"; break;
    case DRIVE_NO_ROOT_DIR: strm << "bad drive path"; break;
    case DRIVE_REMOVABLE:   strm << "removable"; break;
    case DRIVE_FIXED:       strm << "fixed"; break;
    case DRIVE_REMOTE:      strm << "remote"; break;
    case DRIVE_CDROM:       strm << "CD ROM"; break;
    case DRIVE_RAMDISK:     strm << "RAM disk"; break;
    }
    strm << endl;
  }
  {
    //This part of code will determine what this volume is composed of. The returned disk extents are actual disk partitions
    HANDLE volH;
    bool success;
    PVOLUME_DISK_EXTENTS vde;
    DWORD bret;

    volName[wcslen(volName) - 1] = '\0';
    volH = CreateFile(volName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    if (volH == INVALID_HANDLE_VALUE)
    {
      TCHAR msg[MAX_PATH + 1];
      FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), msg, MAX_PATH, NULL);
      strm << L"Open volume error: " << msg << endl;
      return strm.str();
    }
    bret = sizeof(VOLUME_DISK_EXTENTS) + 256 * sizeof(DISK_EXTENT);
    vde = (PVOLUME_DISK_EXTENTS)malloc(bret);
    success = DeviceIoControl(volH, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, (void *)vde, bret, &bret, NULL) != 0;
    if (!success)
      return strm.str();
    for (unsigned i = 0; i < vde->NumberOfDiskExtents; i++)
      strm << L"Volume extent: " << vde->Extents[i].DiskNumber << L" "<< vde->Extents[i].StartingOffset.QuadPart << L" - " << vde->Extents[i].ExtentLength.QuadPart << endl;
    free(vde);
    CloseHandle(volH);
  }

  return strm.str();
}

bool findVolume(WCHAR *volName, int diskno, long long offs, long long len)
{
  HANDLE vol;
  bool success;

  vol = FindFirstVolume(volName, MAX_PATH); //I'm cheating here! I only know volName is MAX_PATH long because I wrote so in enumPartitions findVolume call
  success = vol != INVALID_HANDLE_VALUE;
  while (success)
  {
    //We are now enumerating volumes. In order for this function to work, we need to get partitions that compose this volume
    HANDLE volH;
    PVOLUME_DISK_EXTENTS vde;
    DWORD bret;

    volName[wcslen(volName) - 1] = '\0'; //For this CreateFile, volume must be without trailing backslash
    volH = CreateFile(volName, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, 0);
    volName[wcslen(volName)] = '\\';
    if (volH != INVALID_HANDLE_VALUE)
    {
      bret = sizeof(VOLUME_DISK_EXTENTS) + 256 * sizeof(DISK_EXTENT);
      vde = (PVOLUME_DISK_EXTENTS)malloc(bret);
      if (DeviceIoControl(volH, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0, (void *)vde, bret, &bret, NULL))
      {
        for (unsigned i = 0; i < vde->NumberOfDiskExtents; i++)
          if (vde->Extents[i].DiskNumber == diskno &&
              vde->Extents[i].StartingOffset.QuadPart == offs &&
              vde->Extents[i].ExtentLength.QuadPart == len)
          {
            free(vde);
            CloseHandle(volH);
            FindVolumeClose(vol);
            return true;
          }
      }
      free(vde);
      CloseHandle(volH);
    }

    success = FindNextVolume(vol, volName, MAX_PATH) != 0;
  }
  FindVolumeClose(vol);
  return false;
}

std::vector <HostDevice> GetAvailableHostDevices ()
{
	vector <HostDevice> devices;
	size_t dev0;

	for (int devNumber = 0; devNumber < MAX_HOST_DRIVE_NUMBER; devNumber++)
	{
		wstringstream strm;
		strm << L"\\Device\\Harddisk" << devNumber << L"\\Partition" << 0;
		wstring devPathStr (strm.str());
		const wchar_t *devPath = devPathStr.c_str();
      HANDLE hDev;

	   WCHAR dosDev[MAX_PATH] = {0};
	   WCHAR devName[MAX_PATH] = {0};

	   if (FakeDosNameForDevice ((devNumber+1),devPath, dosDev, sizeof(dosDev), devName, sizeof(devName), FALSE)
         && ((hDev = CreateFileW (devName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
         )
	   {

			HostDevice device;
			device.SystemNumber = devNumber;
			device.Path = devPath;

			// retrieve size using DISK_GEOMETRY
         bool bGeometryValid = false;
			DISK_GEOMETRY deviceGeometry = {0};
			if (	GetDriveGeometry (hDev, &deviceGeometry)
					||	GetPhysicalDriveGeometry (devNumber, &deviceGeometry)
				)
			{
            bGeometryValid = true;
			}

         DISK_PARTITION_INFO_STRUCT info;
			if (GetDeviceInfo (hDev, &info))
            device.Size = info.partInfo.PartitionLength.QuadPart;
         else if (bGeometryValid)
				device.Size = deviceGeometry.Cylinders.QuadPart * (LONGLONG) deviceGeometry.BytesPerSector 
					* (LONGLONG) deviceGeometry.SectorsPerTrack * (LONGLONG) deviceGeometry.TracksPerCylinder;

			UpdateDeviceInfo (device);

			if (bGeometryValid)
				device.Removable = (deviceGeometry.MediaType == RemovableMedia);

			devices.push_back (device);
			dev0 = devices.size() - 1;

         PDRIVE_LAYOUT_INFORMATION_EX layout = NULL;
         BYTE buffer[sizeof (DRIVE_LAYOUT_INFORMATION_EX) + (128 * sizeof (PARTITION_INFORMATION_EX))];
         DWORD bytesRead;
         BOOL bResult = DeviceIoControl (hDev, IOCTL_DISK_GET_DRIVE_LAYOUT_EX, NULL, 0, buffer, sizeof (buffer), &bytesRead, NULL);
         if (bResult)
         {
            layout = (PDRIVE_LAYOUT_INFORMATION_EX) buffer;
            for (DWORD index = 0; index < layout->PartitionCount; index++)
            {
               PARTITION_INFORMATION_EX& partition = layout->PartitionEntry[index];
               DWORD partNumber = partition.PartitionNumber;
               if (  (partition.PartitionStyle == PARTITION_STYLE_MBR)
                  && (partition.Mbr.PartitionType == PARTITION_ENTRY_UNUSED)
                  )
               {
                  continue;
               }

               wstringstream localStrm;
               localStrm << L"\\Device\\Harddisk" << devNumber << L"\\Partition";
               if (partNumber > 0)
                  localStrm << partNumber;
               else
               {
                  // special case of unrecognized partition by Windows
                  localStrm << "??";
               }
			      devPathStr = localStrm.str();
			      devPath = devPathStr.c_str();
                  
               device.Path = devPath;
               device.Size = 0;
               device.MountPoint = L"";
               device.Name = L"";

               if (partNumber > 0)
				      UpdateDeviceInfo (device);
                  
				   // System creates a virtual partition1 for some storage devices without
				   // partition table. We try to detect this case by comparing sizes of
				   // partition0 and partition1. If they match, no partition of the device
				   // is displayed to the user to avoid confusion. Drive letter assigned by
				   // system to partition1 is assigned partition0
               if (partNumber == 1 && (devices[dev0].Size == partition.PartitionLength.QuadPart))
				   {
					   devices[dev0].IsVirtualPartition = true;
					   devices[dev0].MountPoint = device.MountPoint;
					   devices[dev0].Name = device.Name;
					   devices[dev0].Path = device.Path;
					   break;
				   }

				   device.IsPartition = true;
				   device.SystemNumber = partNumber;
				   device.Removable = devices[dev0].Removable;
               device.Size = partition.PartitionLength.QuadPart;

				   if (device.ContainsSystem)
					   devices[dev0].ContainsSystem = true;

				   devices.push_back (device);

				   devices[dev0].Partitions.push_back (device);
            }
			}

         CloseHandle (hDev);
      }

      DefineDosDevice (DDD_REMOVE_DEFINITION, dosDev, devPath);
	}

	// Vista does not create partition links for dynamic volumes so it is necessary to scan \\Device\\HarddiskVolumeX devices
	if (IsWindowsVista())
	{
		for (int devNumber = 0; devNumber < 256; devNumber++)
		{
			wstringstream strm;
			strm << L"\\Device\\HarddiskVolume" << devNumber;
			wstring devPathStr (strm.str());
			const wchar_t *devPath = devPathStr.c_str();

         HANDLE hDev;

	      WCHAR dosDev[MAX_PATH] = {0};
	      WCHAR devName[MAX_PATH] = {0};

	      if (FakeDosNameForDevice ((devNumber+1) * MAX_HOST_PARTITION_NUMBER, devPath, dosDev, sizeof(dosDev), devName, sizeof(devName), FALSE)
            && ((hDev = CreateFileW (devName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL)) != INVALID_HANDLE_VALUE)
            )
	      {
			   DISK_PARTITION_INFO_STRUCT info;
			   if (GetDeviceInfo (hDev, &info) && info.IsDynamic)
			   {
				   HostDevice device;
				   device.DynamicVolume = true;
				   device.IsPartition = true;
				   device.SystemNumber = devNumber;
				   device.Path = devPath;
				   device.Size = info.partInfo.PartitionLength.QuadPart;

				   UpdateDeviceInfo (device);

				   devices.push_back (device);
			   }

            CloseHandle (hDev);
         }

         DefineDosDevice (DDD_REMOVE_DEFINITION, dosDev, devPath);
		}
	}

	return devices;
}


BOOL CALLBACK RawDevicesDlgProc (HWND hwndDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static wchar_t *lpszFileName;		// This is actually a pointer to a GLOBAL array
	static vector <HostDevice> devices;
	static map <int, HostDevice> itemToDeviceMap;
   static wchar_t SysPartitionDevicePath [MAX_PATH];
   static wchar_t SysDriveDevicePath [MAX_PATH];

	WORD lw = LOWORD (wParam);

	switch (msg)
	{
	case WM_INITDIALOG:
		{
			LVCOLUMNW LvCol;
			HWND hList = GetDlgItem (hwndDlg, IDC_DEVICELIST);
			RawDevicesDlgParam* pDlgParam = (RawDevicesDlgParam *) lParam;

			SendMessage (hList,LVM_SETEXTENDEDLISTVIEWSTYLE,0,
				LVS_EX_FULLROWSELECT|LVS_EX_HEADERDRAGDROP|LVS_EX_TWOCLICKACTIVATE|LVS_EX_LABELTIP 
				); 

			memset (&LvCol,0,sizeof(LvCol));               
			LvCol.mask = LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM|LVCF_FMT;  
			LvCol.pszText = L"Device";
			LvCol.cx = CompensateXDPI (186);
			LvCol.fmt = LVCFMT_LEFT;
			SendMessage (hList,LVM_INSERTCOLUMNW,0,(LPARAM)&LvCol);

			LvCol.pszText = L"Drive";  
			LvCol.cx = CompensateXDPI (38);
			LvCol.fmt = LVCFMT_LEFT;
			SendMessage (hList,LVM_INSERTCOLUMNW,1,(LPARAM)&LvCol);

			LvCol.pszText = L"Size";
			LvCol.cx = CompensateXDPI (64);
			LvCol.fmt = LVCFMT_RIGHT;
			SendMessage (hList,LVM_INSERTCOLUMNW,2,(LPARAM)&LvCol);

			LvCol.pszText = L"Label";
			LvCol.cx = CompensateXDPI (128);
			LvCol.fmt = LVCFMT_LEFT;
			SendMessage (hList,LVM_INSERTCOLUMNW,3,(LPARAM)&LvCol);

			devices.clear();
			itemToDeviceMap.clear();

			{
				CWaitCursor busy;
				devices = GetAvailableHostDevices ();
			}

			if (devices.empty())
			{
				::MessageBoxW (hwndDlg, L"Unable to list raw devices installed on your system!", L"Error", MB_ICONHAND);
				EndDialog (hwndDlg, IDCANCEL);
				return 1;
			}

			int line = 1;
			LVITEM item;
			memset (&item, 0, sizeof (item));
			item.mask = LVIF_TEXT;

         for (vector <HostDevice>::iterator It = devices.begin(); It != devices.end(); It++)
         {
            HostDevice& device = *It;

				item.iSubItem = 1;

				if (device.ContainsSystem)
				{
					if (device.IsPartition)
						StringCbCopyW (SysPartitionDevicePath, sizeof (SysPartitionDevicePath), device.Path.c_str());
					else
						StringCbCopyW (SysDriveDevicePath, sizeof (SysDriveDevicePath), device.Path.c_str());
				}

				// Path
				if (!device.IsPartition || device.DynamicVolume)
				{
					if (!device.Floppy && (device.Size == 0) 
						&& (device.IsPartition || device.Partitions.empty() || device.Partitions[0].Size == 0)
						)
						continue;

					if (line > 1)
					{
						ListItemAdd (hList, item.iItem, L"");
						item.iItem = line++;   
					}

					if (device.Floppy || device.DynamicVolume)
					{
						ListItemAdd (hList, item.iItem, (wchar_t *) device.Path.c_str());
					}
					else
					{
						wchar_t s[1024];
						if (device.Removable)
							StringCbPrintfW (s, sizeof(s), L"%s %d", L"Removable Disk", device.SystemNumber);
						else
							StringCbPrintfW (s, sizeof(s), L"%s %d", L"Harddisk", device.SystemNumber);

						if (!device.Partitions.empty())
							StringCbCatW (s, sizeof(s), L":");

						ListItemAdd (hList, item.iItem, s);
					}
				}
				else
				{
					ListItemAdd (hList, item.iItem, (wchar_t *) device.Path.c_str());
				}

				itemToDeviceMap[item.iItem] = device;

				// Size
				if (device.Size != 0)
				{
					wchar_t size[100] = { 0 };
					GetSizeString (device.Size, size, sizeof(size));
					ListSubItemSet (hList, item.iItem, 2, size);
				}

				// Mount point
				if (!device.MountPoint.empty())
					ListSubItemSet (hList, item.iItem, 1, (wchar_t *) device.MountPoint.c_str());

				// Label
				if (!device.Name.empty())
					ListSubItemSet (hList, item.iItem, 3, (wchar_t *) device.Name.c_str());
				else
				{
               TCHAR favoriteLabel [MAX_PATH];
               if (GetVolumeInformation (device.Path.c_str(), favoriteLabel, MAX_PATH, NULL, NULL, NULL, NULL, 0))
						ListSubItemSet (hList, item.iItem, 3, favoriteLabel);
				}

				item.iItem = line++;   
			}

			SendMessageW(hList, LVM_SETCOLUMNWIDTH, 0, MAKELPARAM(LVSCW_AUTOSIZE_USEHEADER, 0));
			SendMessageW(hList, LVM_SETCOLUMNWIDTH, 1, MAKELPARAM(LVSCW_AUTOSIZE_USEHEADER, 0));
			SendMessageW(hList, LVM_SETCOLUMNWIDTH, 2, MAKELPARAM(LVSCW_AUTOSIZE_USEHEADER, 0));
			SendMessageW(hList, LVM_SETCOLUMNWIDTH, 3, MAKELPARAM(LVSCW_AUTOSIZE_USEHEADER, 0));

			lpszFileName = pDlgParam->pszFileName;
			return 1;
		}

	case WM_COMMAND:
	case WM_NOTIFY:
		// catch non-device line selected
		if (msg == WM_NOTIFY && ((LPNMHDR) lParam)->code == LVN_ITEMCHANGED && (((LPNMLISTVIEW) lParam)->uNewState & LVIS_FOCUSED ))
		{
			BOOL bEnableOkButton = FALSE;
			LVITEM LvItem;
			memset(&LvItem,0,sizeof(LvItem));			
			LvItem.mask = LVIF_TEXT | LVIF_PARAM;   
			LvItem.iItem = ((LPNMLISTVIEW) lParam)->iItem;
			LvItem.pszText = lpszFileName;
			LvItem.cchTextMax = MAX_PATH;

			lpszFileName[0] = 0;
			SendMessage (GetDlgItem (hwndDlg, IDC_DEVICELIST), LVM_GETITEM, LvItem.iItem, (LPARAM) &LvItem);
			if (lpszFileName[0] != 0 && lpszFileName[0] != ' ')
			{
            // only select partition not disk
            if (LvItem.iItem != -1 && itemToDeviceMap.find (LvItem.iItem) != itemToDeviceMap.end())
            {
               if (itemToDeviceMap[LvItem.iItem].IsPartition)
				      bEnableOkButton = TRUE;
            }
			}
			EnableWindow (GetDlgItem ((HWND) hwndDlg, IDOK), bEnableOkButton);

			return 1;
		}

		if (msg == WM_COMMAND && lw == IDOK || msg == WM_NOTIFY && ((NMHDR *)lParam)->code == LVN_ITEMACTIVATE)
		{
			int selectedItem = ListView_GetSelectionMark (GetDlgItem (hwndDlg, IDC_DEVICELIST));

			if (selectedItem == -1 || itemToDeviceMap.find (selectedItem) == itemToDeviceMap.end())
				return 1; // non-device line selected	

			const HostDevice selectedDevice = itemToDeviceMap[selectedItem];
         if (selectedDevice.IsPartition)
         {
			   StringCchCopyW (lpszFileName, MAX_PATH, selectedDevice.Path.c_str());

			   EndDialog (hwndDlg, IDOK);
         }
			return 1;
		}

		if ((msg == WM_COMMAND) && (lw == IDCANCEL))
		{
			EndDialog (hwndDlg, IDCANCEL);
			return 1;
		}
		return 0;
	}
	return 0;
}



LRESULT CMainDlg::OnBnClickedSelectDevice(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
   wchar_t szFileName[MAX_PATH+1];
	RawDevicesDlgParam param;
	param.pszFileName = szFileName;
   INT_PTR nResult = DialogBoxParamW (_Module.GetResourceInstance(), MAKEINTRESOURCEW (IDD_DEVICE), m_hWnd,
		(DLGPROC) RawDevicesDlgProc, (LPARAM) & param);
	if (nResult == IDOK)
	{
      SetDlgItemText (IDC_DEVICE, szFileName);
	}

   return 0;
}


LRESULT CMainDlg::OnBnClickedApply(WORD /*wNotifyCode*/, WORD /*wID*/, HWND /*hWndCtl*/, BOOL& /*bHandled*/)
{
   WCHAR szPath[MAX_PATH];

   GetDlgItemText (IDC_DEVICE, szPath, ARRAYSIZE (szPath));

   if (lstrlen (szPath) <= 16 || _wcsnicmp (L"\\Device\\Harddisk", szPath, 16))
      Error (L"Device name invalid", m_hWnd);
   else
   {
	   WCHAR dosDev[MAX_PATH] = {0};
	   WCHAR devName[MAX_PATH] = {0};

	   if (!FakeDosNameForDevice (65 * 33,szPath, dosDev, sizeof(dosDev), devName, sizeof(devName), FALSE))
	   {
         handleWin32Error (m_hWnd);
	   }
      else
      {
         CWaitCursor busy;
         HANDLE dev = OpenPartitionVolume (m_hWnd, devName);
         if (dev != INVALID_HANDLE_VALUE)
         {
            DISK_GEOMETRY driveGeometry;
            DWORD dwResult;

	         if (!DeviceIoControl (dev, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0, &driveGeometry, sizeof (driveGeometry), &dwResult, NULL))
	         {
		         handleWin32Error (m_hWnd);
	         }
            else 
            {
               bool bHadFilesystemBefore = false;
               bool bHasFilesystemNow = false;
               if (ConcealNTFS (dev, bHadFilesystemBefore, bHasFilesystemNow))
               {
                  if (bHadFilesystemBefore)
                     MessageBox (L"VeraCrypt XOR applied successfully.\n\nThe drive filesystem has been concealed", L"Success - Concealed", MB_ICONINFORMATION);
                  else if (bHasFilesystemNow)
                     MessageBox (L"VeraCrypt XOR applied successfully.\n\nThe drive filesystem has been restored", L"Success - Restored", MB_ICONINFORMATION);
                  else
                     MessageBox (L"VeraCrypt XOR applied successfully.\n", L"Success", MB_ICONINFORMATION);
               }
               else
                  handleWin32Error (m_hWnd);                  
	         }

            CloseHandle (dev);
         }
      }
   }

   return 0;
}
