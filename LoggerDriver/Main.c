#include <ntddk.h>
#include "../Logger.h"

#define STDCALL __stdcall

NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pusRegPath);

#ifdef ALLOC_PRAGMA
	#pragma alloc_text (INIT, DriverEntry)
#endif

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	LDestroy();
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "KLogger unloaded\n");
	return;
}

NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegPath)
{
	LErrorCode Code;
	LHANDLE handle1, handle2;
	int i;

	DriverObject->DriverUnload = DriverUnload;
	Code = LInit(RegPath);
	if (Code != LERROR_SUCCESS)
	{
		DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_ERROR_LEVEL, "Failed to init klogger. Error %d\n", Code);
		return STATUS_FAILED_DRIVER_ENTRY;
	}
	DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, "KLogger initialized\n");

	handle1 = LOpen("HANDLE1");
	handle2 = LOpen("HANDLE2");

	for (i = 0; i < 100; i++) {
		LOG(handle1, LDBG, "Debug message %d", i * 2);
		LOG(handle2, LDBG, "Debug message %d", i * 2 + 1);
	}

	LClose(handle1);
	LClose(handle2);

	return STATUS_SUCCESS;
}