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

#define MAX_LOG_FILENAME_SIZE 1024

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
	HANDLE Thread;

#ifdef _KERNEL_MODE
	PVOID DoneEvent;
	PVOID FlushEvent;
	KSPIN_LOCK SpinLock;
#else
	HANDLE DoneEvent;
	HANDLE FlushEvent;
	CRITICAL_SECTION CriticalSection;
#endif
} LoggerStruct;

extern LoggerStruct Logger;

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

#ifdef _KERNEL_MODE
size_t LInitializeParameters(char* FileName, PUNICODE_STRING RegPath);
#else
size_t LInitializeParameters(char* FileName);
#endif
LErrorCode LInitializeObjects(char* FileName);
void LDestroyObjects();
void LSpinlockAcquire();
void LSpinlockRelease();
void LSetFlushEvent();
void LGetTime(unsigned Time[NUM_TIME_PARAMETERS]);

#endif // __LOGGERINTERNAL__