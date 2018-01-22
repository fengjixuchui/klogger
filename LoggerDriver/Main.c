#include <ntddk.h>
#include "../Logger.h"

#define STDCALL __stdcall

NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING pusRegPath);

#ifdef ALLOC_PRAGMA
	#pragma alloc_text (INIT, DriverEntry)
#endif

NTSTATUS DllInitialize(PUNICODE_STRING RegistryPath)
{
	UNREFERENCED_PARAMETER(RegistryPath);
	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "KLogger: DllInitialize called\n");
	return STATUS_SUCCESS;
}

NTSTATUS DllUnload(void)
{
	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "KLogger: DllUnload called\n");
	return STATUS_SUCCESS;
}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	LDestroy();
	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "KLogger unloaded\n");
}

#define SIZE 15
#define THREAD_COUNT 3
#define MSG_COUNT 100

KSTART_ROUTINE ThreadTestMax;
VOID ThreadTestMax(PVOID Param)
{
	int i = 0;
	size_t hndl = (size_t)PsGetCurrentThreadId();
	char hndl_name[SIZE] = { 0 };
	KIRQL newirql = DISPATCH_LEVEL;
	KIRQL oldirql = PASSIVE_LEVEL;
	int result;

	snprintf(hndl_name, SIZE, "%d", (int)hndl);
	hndl = LOpen(hndl_name);

	KeRaiseIrql(newirql, &oldirql);

	for (i = 0; i < MSG_COUNT; i++) {
		LOG(&result, hndl, LDBG, "Debug message %d", i);
		i++;

		if (result != TRUE)
			break;
	}

	KeLowerIrql(oldirql);

	return;
}

void test_max() {
	HANDLE threads[THREAD_COUNT] = { 0 };
	int i = 0;
	NTSTATUS Status;
	PVOID waitobj[THREAD_COUNT] = { 0 };

	for (i = 0; i < THREAD_COUNT; i++) {
		Status = PsCreateSystemThread(&threads[i], 0, NULL, NULL, NULL, ThreadTestMax, NULL);
		if (!NT_SUCCESS(Status))
			return;
	}

	for (i = 0; i < THREAD_COUNT; i++) {
		Status = ObReferenceObjectByHandle(threads[i], EVENT_ALL_ACCESS, NULL, KernelMode, &waitobj[i], NULL);
		if (!NT_SUCCESS(Status))
			return;
	}

	for (i = 0; i < THREAD_COUNT; i++)
		Status = KeWaitForSingleObject(&waitobj[i], Executive, KernelMode, FALSE, NULL);


}

NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegPath)
{
	LErrorCode Code;
	UNREFERENCED_PARAMETER(RegPath);

	DriverObject->DriverUnload = DriverUnload;
	Code = LInit(RegPath);
	if (Code != LERROR_SUCCESS)
	{
		char* Error;
		switch (Code)
		{
		case LERROR_ALREADY_INITIALIZED:
			Error = "Already initialized";
			break;
		case LERROR_INTERNAL:
			Error = "Internal error";
			break;
		case LERROR_ANOTHER_THREAD_INITIALIZING:
			Error = "Another thread initializing";
			break;
		case LERROR_REGISTRY:
			Error = "Registry error";
			break;
		case LERROR_MEMORY_ALLOC:
			Error = "Memory alloc error";
			break;
		case LERROR_CREATE_EVENT:
			Error = "Create event error";
			break;
		case LERROR_OPEN_FILE:
			Error = "Open file error";
			break;
		case LERROR_CREATE_THREAD:
			Error = "Create thread error";
			break;
		default:
			Error = "Unknown error";
			break;
		}
		DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_ERROR_LEVEL, "Failed to init klogger. Error %d (%s)\n", Code, Error);
		return STATUS_FAILED_DRIVER_ENTRY;
	}
	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "KLogger initialized\n");


	//

	return STATUS_SUCCESS;
}