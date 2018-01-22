#include <ntddk.h>
#include "../Logger.h"
#include "../LoggerInternal.h"

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

#define SIZE 15
#define THREAD_COUNT 3
#define MSG_COUNT 10000

KSTART_ROUTINE ThreadTest;
VOID ThreadTest(PVOID Param)
{
	UNREFERENCED_PARAMETER(Param);
	int i = 0;
	size_t hndl = (size_t)PsGetCurrentThreadId();
	LHANDLE h;
	char hndl_name[SIZE] = { 0 };
	KIRQL newirql = DISPATCH_LEVEL;
	KIRQL oldirql = PASSIVE_LEVEL;
	int result;

	snprintf(hndl_name, SIZE, "%d", (int)hndl);
	h = LOpen(hndl_name);

	KeRaiseIrql(newirql, &oldirql);

	for (i = 0; i < MSG_COUNT; i++) {
		result = LPrint(h, LDBG, "Debug message", sizeof("Debug message"));
		i++;

		if (result != TRUE)
			break;
	}

	KeLowerIrql(oldirql);

	LClose(h);
}

void test() {
	HANDLE threads[THREAD_COUNT] = { 0 };
	int i = 0;
	NTSTATUS Status;
	PVOID waitobj[THREAD_COUNT] = { 0 };
	for (i = 0; i < THREAD_COUNT; i++) {
		Status = PsCreateSystemThread(&threads[i], 0, NULL, NULL, NULL, ThreadTest, NULL);
		if (!NT_SUCCESS(Status))
			return;
	}

	for (i = 0; i < THREAD_COUNT; i++) {
		Status = ObReferenceObjectByHandle(threads[i], EVENT_ALL_ACCESS, NULL, KernelMode, &waitobj[i], NULL);
		if (!NT_SUCCESS(Status))
			return;
		ZwClose(threads[i]);
	}

	// it's not good code, but it's for testing
	for (i = 0; i < THREAD_COUNT; i++)
		Status = KeWaitForSingleObject(waitobj[i], Executive, KernelMode, FALSE, NULL);

	for (i = 0; i < THREAD_COUNT; i++)
		ObDereferenceObject(waitobj[i]);
}

NTSTATUS STDCALL DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegPath)
{
	UNREFERENCED_PARAMETER(RegPath);

	DriverObject->DriverUnload = DriverUnload;

	DbgPrintEx(DPFLTR_DEFAULT_ID, DPFLTR_INFO_LEVEL, "Test driver initialized\n");

	test();

	return STATUS_SUCCESS;
}