#include <ntddk.h>
#include "Aphy2Queue.h"
#define CODE_EVENT CTL_CODE(FILE_DEVICE_UNKNOWN, 0x900, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define CODE_CREATE_PROCESS_NOTIFY CTL_CODE(FILE_DEVICE_UNKNOWN, 0x901, METHOD_BUFFERED, FILE_ANY_ACCESS)
PDEVICE_OBJECT DeviceObject;
UNICODE_STRING DeviceName = RTL_CONSTANT_STRING(L"\\Device\\Aphy2ProcMonDrvCtlDevice"), SymbolicPathName = RTL_CONSTANT_STRING(L"\\??\\Aphy2ProcMonDrvCtlSymbolic");
PUNICODE_STRING RegPath;
PKEVENT ProcessNotifyEvent, DriverUnloadEvent;
typedef struct _APHY2_CREATE_PROCESS_INFO
{
	HANDLE ProcessID;
	HANDLE ParentProcessID;
	PVOID PointerToEPROCESS;
	UCHAR Terminate;
	WCHAR CommandLine[513];
	WCHAR ImageFileName[261];
}APHY2_CREATE_PROCESS_INFO, * PAPHY2_CREATE_PROCESS_INFO;
//APHY2_CREATE_PROCESS_INFO LatestCreateProcessInfo;
APHY2_QUEUE<APHY2_CREATE_PROCESS_INFO> CreateProcessQueue;
VOID CreateProcessNotifyRoutineEx(PEPROCESS Process, HANDLE ProcessID, PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	APHY2_CREATE_PROCESS_INFO CreateProcessInfo;
	memset(&CreateProcessInfo, 0, sizeof(APHY2_CREATE_PROCESS_INFO));
	if (CreateInfo != NULL)
	{
		wcsncpy(CreateProcessInfo.CommandLine, CreateInfo->CommandLine->Buffer, CreateInfo->CommandLine->Length / 2);
		wcsncpy(CreateProcessInfo.ImageFileName, CreateInfo->ImageFileName->Buffer, CreateInfo->ImageFileName->Length / 2);
		CreateProcessInfo.CommandLine[CreateInfo->CommandLine->Length / 2] = 0;
		CreateProcessInfo.ImageFileName[CreateInfo->ImageFileName->Length / 2] = 0;
		CreateProcessInfo.ParentProcessID = CreateInfo->ParentProcessId;
	}
	else
	{
		CreateProcessInfo.Terminate = 1;
	}
	CreateProcessInfo.PointerToEPROCESS = Process;
	CreateProcessInfo.ProcessID = ProcessID;
	//LatestCreateProcessInfo = CreateProcessInfo;
	CreateProcessQueue.Aphy2QueuePush(CreateProcessInfo);
	if(ProcessNotifyEvent)
		KeSetEvent(ProcessNotifyEvent, IO_NO_INCREMENT, FALSE);
}
VOID DriverUnload(PDRIVER_OBJECT Driver)
{
	DbgPrint("Aphy2进程监视驱动卸载");
	IoDeleteSymbolicLink(&SymbolicPathName);
	IoDeleteDevice(DeviceObject);
	PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineEx, TRUE);
	if (DriverUnloadEvent)KeSetEvent(DriverUnloadEvent, 0, 0);
	__try
	{
		if(ProcessNotifyEvent)
			ObDereferenceObject(ProcessNotifyEvent);
	}__except(EXCEPTION_EXECUTE_HANDLER){}
}
NTSTATUS DriverDispath(PDEVICE_OBJECT Device, PIRP irp)
{
	NTSTATUS status = STATUS_SUCCESS;
	irp->IoStatus.Information = 0;
	if (Device != DeviceObject)
	{
		status = STATUS_UNSUCCESSFUL;
	}
	else
	{
		PIO_STACK_LOCATION irpsp = IoGetCurrentIrpStackLocation(irp);
		if (irpsp->MajorFunction == IRP_MJ_DEVICE_CONTROL)
		{
			if (irpsp->Parameters.DeviceIoControl.IoControlCode == CODE_EVENT)
			{
				if (irpsp->Parameters.DeviceIoControl.InputBufferLength != sizeof(HANDLE) || irpsp->Parameters.DeviceIoControl.OutputBufferLength != 0)
				{
					status = STATUS_INVALID_PARAMETER;
				}
				HANDLE hProcessNotifyEvent = (HANDLE)*((PULONG64)irp->AssociatedIrp.SystemBuffer);
				status = ObReferenceObjectByHandle(hProcessNotifyEvent, EVENT_MODIFY_STATE, *ExEventObjectType, KernelMode, (PVOID*)&ProcessNotifyEvent, 0);
				if (status != STATUS_SUCCESS)
				{

					irpsp->Parameters.DeviceIoControl.OutputBufferLength = 0;
					irp->IoStatus.Information = 0;
					irp->IoStatus.Status = status;
					IoCompleteRequest(irp, IO_NO_INCREMENT);
					return status;
				}
				irpsp->Parameters.DeviceIoControl.OutputBufferLength = 0;
				irp->IoStatus.Information = 0;
			}
			else if (irpsp->Parameters.DeviceIoControl.IoControlCode == CODE_CREATE_PROCESS_NOTIFY)
			{
				if (irpsp->Parameters.DeviceIoControl.InputBufferLength != sizeof(APHY2_CREATE_PROCESS_INFO) || irpsp->Parameters.DeviceIoControl.OutputBufferLength != sizeof(APHY2_CREATE_PROCESS_INFO))
				{
					status = STATUS_INVALID_PARAMETER;
				}
				PAPHY2_CREATE_PROCESS_INFO CreateProcessInfo = (PAPHY2_CREATE_PROCESS_INFO)irp->AssociatedIrp.SystemBuffer;
				//memcpy(CreateProcessInfo, &LatestCreateProcessInfo, sizeof(APHY2_CREATE_PROCESS_INFO));
				CreateProcessQueue.Aphy2QueuePop(CreateProcessInfo);
				irp->IoStatus.Information = sizeof(APHY2_CREATE_PROCESS_INFO);
			}
		}
		else if (irpsp->MajorFunction == IRP_MJ_CLOSE)
		{
			__try
			{
				ObDereferenceObject(ProcessNotifyEvent);
				ProcessNotifyEvent = NULL;
			}__except(EXCEPTION_EXECUTE_HANDLER){}
		}
	}
	irp->IoStatus.Status = status;
	IoCompleteRequest(irp, IO_NO_INCREMENT);
	return status;
}
extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT Driver, PUNICODE_STRING RegistryPath)
{
	Driver->DriverUnload = DriverUnload;
	RegPath = RegistryPath;
	DbgPrint("Aphy2进程监视驱动加载");
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Aphy2进程监视驱动加载");
	NTSTATUS status = STATUS_SUCCESS;
	status = IoCreateDevice(Driver, 0, &DeviceName, FILE_DEVICE_UNKNOWN, 0, TRUE, &DeviceObject);
	if (NT_SUCCESS(status))
	{
		return status;
	}
	status = IoCreateSymbolicLink(&SymbolicPathName, &DeviceName);
	if (!NT_SUCCESS(status))
	{
		IoDeleteDevice(DeviceObject);
		return status;
	}
	for (int i = 0; i < IRP_MJ_MAXIMUM_FUNCTION; i++)Driver->MajorFunction[i] = DriverDispath;
	if (CreateProcessQueue.Aphy2QueueError())
	{
		IoDeleteSymbolicLink(&SymbolicPathName);
		IoDeleteDevice(DeviceObject);
		return status;
	}
	status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutineEx, FALSE);
	if (!NT_SUCCESS(status))
	{
		IoDeleteSymbolicLink(&SymbolicPathName);
		IoDeleteDevice(DeviceObject);
		return status;
	}
	return status;
}