#include <Ntifs.h>
#include "../LoggerInternal.h"
#include "../Logger.h"

size_t LInitializeParameters(char* FileName)
{
	size_t Size = 4096;
	strncpy(FileName, "C:\\Users\\Jeka\\Desktop\\Log.txt", MAX_LOG_FILENAME_SIZE);

	Logger.Level = LDBG;
	Logger.OutputDbg = TRUE;
	Logger.IdentificatorsSize = 10;
	Logger.Timeout = 10 * 1000;
	Logger.FlushPercent = 90;

	return Size;
}

KSTART_ROUTINE ThreadFunction;
VOID ThreadFunction(PVOID Param)
{
	NTSTATUS Status;
	IO_STATUS_BLOCK sb;
	size_t Size;
	const char* Buffer;
	LARGE_INTEGER Timeout;
	PVOID Objects[2];
	UNREFERENCED_PARAMETER(Param);
	Objects[0] = Logger.DoneEvent;
	Objects[1] = Logger.FlushEvent;

	Timeout.QuadPart = -10 * 1000 * Logger.Timeout;

	for (;;)
	{
		Status = KeWaitForMultipleObjects(2, Objects, WaitAny, Executive, KernelMode, FALSE, &Timeout, NULL);

		while (!RBEmpty(&Logger.RB))
		{
			Buffer = RBRead(&Logger.RB, &Size);
			if (!Buffer)
				break;

			ZwWriteFile(Logger.File, NULL, NULL, NULL, &sb, (PVOID)Buffer, (ULONG)Size, NULL, NULL);
			if (Logger.OutputDbg)
				DbgPrint("%.*s", (int)Size, Buffer);
		}
		if (NT_SUCCESS(Status) && Status == STATUS_WAIT_0 + 0) // DoneEvent
			break;
	}
}

static PVOID CreateEvent(PCWSTR Name)
{
	NTSTATUS Status;
	HANDLE hEvent;
	PVOID pEvent;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;

	RtlInitUnicodeString(&us, Name);
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);
	Status = ZwCreateEvent(&hEvent, EVENT_ALL_ACCESS, &oa, NotificationEvent, FALSE);
	if (!NT_SUCCESS(Status))
		return NULL;
	Status = ObReferenceObjectByHandle(hEvent, EVENT_ALL_ACCESS, NULL, KernelMode, &pEvent, NULL);
	if (!NT_SUCCESS(Status))
	{
		ZwClose(hEvent);
		return NULL;
	}
	return pEvent;
}

LErrorCode LInitializeObjects(char* FileName)
{
	NTSTATUS Status;
	OBJECT_ATTRIBUTES oa;
	UNICODE_STRING us;
	IO_STATUS_BLOCK sb;
	UNREFERENCED_PARAMETER(FileName);

	Logger.DoneEvent = CreateEvent(L"\\BaseNamedObjects\\LoggerDoneEvent");
	if (!Logger.DoneEvent)
		return LERROR_CREATE_EVENT;
	Logger.FlushEvent = CreateEvent(L"\\BaseNamedObjects\\LoggerFlushEvent");
	if (!Logger.FlushEvent)
	{
		ObDereferenceObject(Logger.DoneEvent);
		return LERROR_CREATE_EVENT;
	}
	RtlInitUnicodeString(&us, L"C:\\Users\\jeka\\Desktop\\Log.txt");
	InitializeObjectAttributes(&oa, &us, OBJ_CASE_INSENSITIVE, NULL, NULL);
	Status = ZwCreateFile(&Logger.File, GENERIC_READ, &oa, &sb, NULL, FILE_ATTRIBUTE_NORMAL, 
		FILE_SHARE_READ, FILE_SUPERSEDE, FILE_SEQUENTIAL_ONLY, NULL, 0);
	if (!NT_SUCCESS(Status))
	{
		ObDereferenceObject(Logger.DoneEvent);
		ObDereferenceObject(Logger.FlushEvent);
		return LERROR_OPEN_FILE;
	}
	Status = PsCreateSystemThread(&Logger.Thread, 0, NULL, NULL, NULL, ThreadFunction, NULL);
	if (!NT_SUCCESS(Status))
	{
		ObDereferenceObject(Logger.DoneEvent);
		ObDereferenceObject(Logger.FlushEvent);
		ZwClose(Logger.File);
		return LERROR_CREATE_THREAD;
	}
	KeInitializeSpinLock(&Logger.SpinLock);
	return LERROR_SUCCESS;
}

void LDestroyObjects()
{
	// TODO:
}

void LSpinlockAcquire()
{
	// TODO:
}

void LSpinlockRelease()
{
	// TODO:
}

void LSetFlushEvent()
{
	// TODO: KeInsertQueueDpc
	KeSetEvent(Logger.FlushEvent, 0, FALSE);
}

void LGetTime(unsigned Time[NUM_TIME_PARAMETERS])
{
	// TODO:
	UNREFERENCED_PARAMETER(Time);
}