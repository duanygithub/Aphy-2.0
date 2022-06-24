#pragma once
#pragma once
#include <Windows.h>
typedef union _APHY2_EXCEPTION
{
	struct
	{
		WORD Code;
		WORD DeviceCode : 12;
		WORD Reserved : 1;
		WORD Flag : 1;
		WORD Status : 2;
	}Bits;
	DWORD ExceptionCode;
}APHY2_EXCEPTION;
BOOL Aphy2ExceptionInit(APHY2_EXCEPTION& Exception, WORD Code, WORD DeviceCode, bool IsCustomerCode, WORD Status)
{
	if (DeviceCode > 80 || Status > 3)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}
	Exception.Bits.Code = Code;
	Exception.Bits.DeviceCode = DeviceCode;
	Exception.Bits.Flag = IsCustomerCode;
	Exception.Bits.Status = Status;
	return TRUE;
}
#define CheckError()\
	if (GetLastError() != 0)\
	{\
		DWORD dwError = GetLastError();\
		dwError <<= 16;\
		dwError >>= 16;\
		APHY2_EXCEPTION Exception;\
		Aphy2ExceptionInit(Exception, dwError, 0, 1, 3);\
		RaiseException(Exception.ExceptionCode, 0, 0, NULL);\
		exit(-1);\
	}