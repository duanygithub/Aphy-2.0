#include <Windows.h>
#include <winternl.h>
#include <iostream>
#include <iomanip>
#include <conio.h>
#include <process.h>
#include <winternl.h>
//#include <list>
#include "Aphy2Error.h"
#define CODE_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CODE_CREATE_PROCESS_NOTIFY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CODE_EVENT2 CTL_CODE(FILE_DEVICE_UNKNOWN, 0x902, METHOD_BUFFERED, FILE_ANY_ACCESS)
using namespace std;
HANDLE hDevice, hMySelf, hProcessNotifyEvent, hDriverUnloadEvent;
typedef struct _APHY2_CREATE_PROCESS_INFO
{
	HANDLE ProcessID;
	HANDLE ParentProcessID;
	PVOID PointerToEPROCESS;
	UCHAR Terminate;
	WCHAR CommandLine[513];
	WCHAR ImageFileName[261];
}APHY2_CREATE_PROCESS_INFO, * PAPHY2_CREATE_PROCESS_INFO;
//list<APHY2_CREATE_PROCESS_INFO> ActiveProcesses;
BOOL bRun = 1;
int main()
{
	DWORD dwError = GetLastError();
	__try
	{
		TCHAR FileName[MAX_PATH];
		GetModuleFileName(NULL, FileName, MAX_PATH);
		hMySelf = CreateFile(FileName, FILE_GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		CheckError();
		hDevice = CreateFile(TEXT("\\\\.\\Aphy2ProcMonDrvCtlSymbolic"), FILE_GENERIC_READ | FILE_GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_SYSTEM, NULL);
		CheckError();
		hProcessNotifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		CheckError();
		DWORD dwRetLength = 0;
		DeviceIoControl(hDevice, CODE_EVENT, &hProcessNotifyEvent, sizeof(HANDLE), &hProcessNotifyEvent, 0, &dwRetLength, NULL);
		CheckError();
		while (bRun)
		{
			WaitForSingleObject(hProcessNotifyEvent, INFINITE);
			APHY2_CREATE_PROCESS_INFO Info = { 0 };
			DeviceIoControl(hDevice, CODE_CREATE_PROCESS_NOTIFY, &Info, sizeof(APHY2_CREATE_PROCESS_INFO), &Info, sizeof(APHY2_CREATE_PROCESS_INFO), &dwRetLength, NULL);
			if (Info.Terminate)
			{
				cout << "进程退出,PID:" << Info.ProcessID << ",EPROCESS地址:" << hex << Info.PointerToEPROCESS << endl;
//				for (auto Process : ActiveProcesses)
//				{
//					if (Info.ProcessID == Process.ProcessID)ActiveProcesses.remove(Process);
//				}
			}
			else
			{
				cout << "进程创建,PID:" << Info.ProcessID;
				wcout << "进程名:" << Info.ImageFileName << "父进程PID:" << Info.ParentProcessID << endl;
//				ActiveProcesses.push_back(Info);
			}
		}
	}
	__finally
	{
		CloseHandle(hProcessNotifyEvent);
		CloseHandle(hDevice);
		CloseHandle(hMySelf);
		return GetLastError();
	}
}