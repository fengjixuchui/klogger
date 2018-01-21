#include "../LoggerInternal.h"
#include "../Logger.h"

size_t LInitializeParameters(char* FileName)
{
	size_t Size = 4096;
	strncpy(FileName, "D:\\tmp\\log.txt", MAX_LOG_FILENAME_SIZE);

	Logger->Level = LDBG;
	Logger->OutputDbg = TRUE;
	Logger->IdentificatorsSize = 10;
	Logger->Timeout = 10 * 1000;
	Logger->FlushPercent = 50;

	return Size;
}

DWORD WINAPI ThreadFunction(LPVOID Param)
{
	DWORD Result;
	HANDLE Objects[2] = { Logger->DoneEvent, Logger->FlushEvent };
	for (;;)
	{
		Result = WaitForMultipleObjects(2, Objects, FALSE, Logger->Timeout);
		if (Result == WAIT_FAILED)
		{
			printf("WaitForMultipleObjects failed. Continue\n");
			continue;
		}

		char* ptr;
		size_t size;
		while (ptr = RBGetReadPTR(&Logger->RB, &size))
		{
			printf("%.*s", (int)size, ptr);
			RBRelease(&Logger->RB, size);
		}

		if (Result == WAIT_OBJECT_0 + 0) // DoneEvent
			break;
	}
	return 0;
}

LErrorCode LInitializeObjects(char* FileName)
{
	Logger->DoneEvent = CreateEvent(NULL, FALSE, FALSE, "DoneEvent");
	if (!Logger->DoneEvent)
		return LERROR_CREATE_EVENT;
	Logger->FlushEvent = CreateEvent(NULL, FALSE, FALSE, "FlushEvent");
	if (!Logger->FlushEvent)
	{
		CloseHandle(Logger->DoneEvent);
		return LERROR_CREATE_EVENT;
	}

	Logger->File = CreateFile(FileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (Logger->File == INVALID_HANDLE_VALUE)
	{
		CloseHandle(Logger->DoneEvent);
		CloseHandle(Logger->FlushEvent);
		return LERROR_OPEN_FILE;
	}

	Logger->Thread = CreateThread(NULL, 0, ThreadFunction, NULL, 0, NULL);
	if (!Logger->Thread)
	{
		CloseHandle(Logger->DoneEvent);
		CloseHandle(Logger->FlushEvent);
		CloseHandle(Logger->File);
		return LERROR_CREATE_THREAD;
	}
	InitSpinLock(&Logger->spinlock);

	return LERROR_SUCCESS;
}

void LDestroyObjects()
{
	SetEvent(Logger->DoneEvent);
	WaitForSingleObject(Logger->Thread, INFINITE);
	CloseHandle(Logger->DoneEvent);
	CloseHandle(Logger->FlushEvent);
	CloseHandle(Logger->Thread);
	CloseHandle(Logger->File);
	DestroySpinLock(&Logger->spinlock);
}

void LSetFlushEvent()
{
	SetEvent(Logger->FlushEvent);
}

void LGetTime(unsigned Time[NUM_TIME_PARAMETERS])
{
	SYSTEMTIME LocalTime = { 0 };
	GetLocalTime(&LocalTime);

	Time[TIME_YEAR] = LocalTime.wYear;
	Time[TIME_MONTH] = LocalTime.wMonth;
	Time[TIME_DAY] = LocalTime.wDay;
	Time[TIME_HOUR] = LocalTime.wHour;
	Time[TIME_MINUTE] = LocalTime.wMinute;
	Time[TIME_SECOND] = LocalTime.wSecond;
	Time[TIME_MILLISECONDS] = LocalTime.wYear;
}