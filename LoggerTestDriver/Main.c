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
	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "Test driver unloaded\n");
}

NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegPath)
{
	LHANDLE handle1, handle2;
	int i;
	UNREFERENCED_PARAMETER(RegPath);

	DriverObject->DriverUnload = DriverUnload;

	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "Test driver initialized\n");

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