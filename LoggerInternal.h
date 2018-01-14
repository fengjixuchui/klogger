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

	LogLevel FileLevel;
	LogLevel OutputLevel;
	unsigned Timeout;
	unsigned FlushPercent;

	HANDLE File;
	HANDLE DoneEvent;
	HANDLE FlushEvent;
	HANDLE Thread;

#ifdef _KERNEL_MODE
	// TODO: spinlock
#else
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

size_t LInitializeParameters(char* FileName);
LErrorCode LInitializeObjects(char* FileName);
void LDestroyObjects();
void LSpinlockLock();
void LSpinlockFree();
void LSetFlushEvent();
void LGetTime(unsigned Time[NUM_TIME_PARAMETERS]);

#endif // __LOGGERINTERNAL__