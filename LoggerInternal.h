#ifndef __LOGGERINTERNAL__
#define __LOGGERINTERNAL__

#ifdef _KERNEL_MODE
#include <ntddk.h>
#else
#include <windows.h>
#endif

#include "Main.h"
#include "RingBuffer.h"
#include "Logger.h"

#define MAX_LOG_FILENAME_SIZE 4096

typedef struct
{
	BOOL Initialized;

	RingBuffer RB;
	char* Identificators;
	size_t NumIdentificators;
	size_t IdentificatorsSize;

	LogLevel Level;
	BOOL OutputDbg;
	unsigned Timeout;
	unsigned FlushPercent;

	HANDLE File;
	SpinLockObject SpinLock;

#ifdef _KERNEL_MODE
	KDPC FlushDpc;
	PVOID Thread;
	PVOID DoneEvent;
	PVOID FlushEvent;
#else
	HANDLE Thread;
	HANDLE DoneEvent;
	HANDLE FlushEvent;
#endif
} LoggerStruct;

extern LoggerStruct* Logger;

enum TimeParameters
{
	TIME_YEAR,
	TIME_MONTH,
	TIME_DAY,
	TIME_HOUR,
	TIME_MINUTE,
	TIME_SECOND,
	TIME_MILLISECONDS,
	NUM_TIME_PARAMETERS
};

typedef struct
{
	BOOL Status;

	size_t RingBufferSize;
	BOOL NonPagedPool;
	BOOL WaitAtPassive;
} LInitializationParameters;

#ifdef _KERNEL_MODE
LInitializationParameters LInitializeParameters(WCHAR* FileName, PUNICODE_STRING RegPath);
#else
LInitializationParameters LInitializeParameters(WCHAR* FileName);
#endif
LErrorCode LInitializeObjects(WCHAR* FileName);
void LDestroyObjects();
void LSetFlushEvent();
void LGetTime(unsigned Time[NUM_TIME_PARAMETERS]);

#endif // __LOGGERINTERNAL__