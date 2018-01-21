#include <stdio.h>
#include "Main.h"
#include "Logger.h"
#include "RingBuffer.h"
#include "LoggerInternal.h"

EXPORT_FUNC  char __String[MAX_LOG_SIZE];

#ifndef MIN
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#endif
#ifndef MAX
#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#endif

static size_t CalculateReservedBytes();

#if defined _KERNEL_MODE && defined ALLOC_PRAGMA
	#pragma alloc_text(INIT, CalculateReservedBytes)
	#pragma alloc_text(INIT, LInit)
	#pragma alloc_text(INIT, LInitializeParameters)
	#pragma alloc_text(INIT, LInitializeObjects)
	#pragma alloc_text(INIT, RBInit)
#endif

#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define MAX_IDENTIFICATOR_NAME_SIZE 9
#define MAX_IDENTIFICATOR_MEMORY_SIZE (MAX_IDENTIFICATOR_NAME_SIZE + 1)
#define MIN_RING_BUFFER_SIZE 1024

#define LOG_FORMAT "%3s [%02u.%02u.%02u %02u:%02u:%02u:%03u] %-" STR(MAX_IDENTIFICATOR_NAME_SIZE) "s | "
#define LOG_FULL_FORMAT "\nLog is full! %" STR(MAX_IDENTIFICATOR_NAME_SIZE) "s log message was clipped\n"
#define LOG_FULL_STRING_SIZE 1024

#define LHANDLE_LOGGER ((LHANDLE) 0)

LoggerStruct* Logger = NULL;

static size_t CalculateReservedBytes()
{
	char TempTag[MAX_IDENTIFICATOR_NAME_SIZE + 1] = "";
	int i;
	char Temp[LOG_FULL_STRING_SIZE];
	size_t ReservedBytes;

	for (i = 0; i < MAX_IDENTIFICATOR_NAME_SIZE; i++)
		TempTag[i] = ' ';
	ReservedBytes = snprintf(Temp, LOG_FULL_STRING_SIZE, LOG_FULL_FORMAT, TempTag);
	if (ReservedBytes >= LOG_FULL_STRING_SIZE)
		return (size_t) -1;

	return ReservedBytes;
}

#ifdef _KERNEL_MODE
EXPORT_FUNC LErrorCode LInit(PUNICODE_STRING RegPath)
#else
EXPORT_FUNC LErrorCode LInit()
#endif
{
	size_t ReservedBytes;
	WCHAR* FileName;
	LInitializationParameters Parameters;
	LErrorCode Code;
	unsigned i;
	POOL_TYPE pool;
	LoggerStruct* logger_try;
	PVOID result;

	pool = NonPagedPoolNx;
	if (Logger != NULL) {
		if (Logger->Initialized == TRUE)
			return LERROR_ALREADY_INITIALIZED;

		return LERROR_ANOTHER_THREAD_INITIALIZING;
	}

	logger_try =  MemoryAlloc(sizeof(LoggerStruct), pool);
	if (!logger_try)
		return LERROR_MEMORY_ALLOC;
	logger_try->Initialized = FALSE;

	result = InterlockedCompareExchangePointer(&Logger, logger_try, NULL);

	if (result != NULL) {
		if(Logger->Initialized == TRUE)
			return LERROR_ALREADY_INITIALIZED;

		return LERROR_ANOTHER_THREAD_INITIALIZING;
	}	

	FileName = MemoryAlloc(MAX_LOG_FILENAME_SIZE * sizeof (WCHAR), PagedPool);
	if (!FileName)
	{
		MemoryFree(logger_try, sizeof(LoggerStruct));
		return LERROR_MEMORY_ALLOC;
	}
	ReservedBytes = CalculateReservedBytes();
	if (ReservedBytes == -1) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_INTERNAL;
	}
	InitSpinLock(&Logger->SpinLock);
#ifdef _KERNEL_MODE
	Parameters = LInitializeParameters(FileName, RegPath);
#else
	Parameters = LInitializeParameters(FileName);
#endif
	if (!Parameters.Status) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_REGISTRY;
	}
	pool = Parameters.NonPagedPool ? NonPagedPoolNx : PagedPool;
	Logger->IdentificatorsSize = MAX(Logger->IdentificatorsSize, 1);
	Logger->Level = MIN(Logger->Level, LOG_LEVEL_NONE);
	Logger->FlushPercent = MAX(MIN(Logger->FlushPercent, 100), 1);
	Parameters.RingBufferSize = MAX(Parameters.RingBufferSize, MIN_RING_BUFFER_SIZE);
	
	Logger->IdentificatorsSize++; // one identificator for logger
	Logger->NumIdentificators = 1;
	Logger->Identificators = MemoryAlloc(Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE, pool);
	if (!Logger->Identificators) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_MEMORY_ALLOC;
	}
	for (i = 0; i < Logger->IdentificatorsSize; i++)
		Logger->Identificators[i * MAX_IDENTIFICATOR_MEMORY_SIZE] = 0;
	strncpy(Logger->Identificators, "LOGGER", MAX_IDENTIFICATOR_NAME_SIZE);


	if (!RBInit(&Logger->RB, Parameters.RingBufferSize, ReservedBytes, Parameters.WaitAtPassive, pool))
	{
		MemoryFree(Logger->Identificators, Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE);
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_MEMORY_ALLOC;
	}

	Code = LInitializeObjects(FileName);
	if (Code != LERROR_SUCCESS)
	{
		MemoryFree(Logger->Identificators, Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE);
		Logger = NULL;
		RBDestroy(&(logger_try->RB));
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return Code;
	}

	Logger->Initialized = TRUE;
	LOG(LHANDLE_LOGGER, LINF, "Log inited\nFilename: %ws\nFlush: %u%%\nNum identificators: %d\nLevel: %d\nOutput dbg: %s\nTimeout: %dms\n"
		"Ring buffer size: %u\nWait at passive: %s\nMemory pool type: %s", 
		FileName, Logger->FlushPercent, (int)Logger->IdentificatorsSize - 1, (int)Logger->Level, 
		Logger->OutputDbg ? "YES" : "NO", Logger->Timeout, (unsigned int) Parameters.RingBufferSize, 
		Parameters.WaitAtPassive ? "YES" : "NO", Parameters.NonPagedPool ? "NonPaged" : "Paged");
	MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));

	return LERROR_SUCCESS;
}

EXPORT_FUNC void LDestroy()  //Library user responsibility not to call before everyone stopped logging 
{
	if (Logger == NULL)
		return;

	if (Logger->Initialized == FALSE)
		return;

	if (Logger->NumIdentificators != 1 /* logger identificator */)
		LOG(LHANDLE_LOGGER, LWRN, "%u identificators not closed!", (int)Logger->NumIdentificators - 1);
	LOG(LHANDLE_LOGGER, LINF, "Log destroyed\n");

	LDestroyObjects();
	DestroySpinLock(&Logger->SpinLock);
	MemoryFree(Logger->Identificators, Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE);
	RBDestroy(&(Logger->RB));

	MemoryFree(Logger, sizeof(LoggerStruct));

	Logger = NULL;
}

EXPORT_FUNC BOOL LIsInitialized()
{
	if (Logger == NULL)
		return FALSE;

	return Logger->Initialized;
}

EXPORT_FUNC LHANDLE LOpen(const char* Name)
{
	size_t FreeIdetificator;
	unsigned i;
	KIRQL irql;
	UNREFERENCED_PARAMETER(irql);

	if (Logger == NULL)
		return LHANDLE_INVALID;

	if (!Logger->Initialized)
		return LHANDLE_INVALID;

	AcquireSpinLock(&Logger->SpinLock, &irql);

	FreeIdetificator = (size_t) -1;
	for (i = 0; i < Logger->IdentificatorsSize; i++)
	{
		if (!Logger->Identificators[i * MAX_IDENTIFICATOR_MEMORY_SIZE])
		{
			FreeIdetificator = i;
			break;
		}
	}
	if (FreeIdetificator == (size_t) -1)
	{
		ReleaseSpinLock(&Logger->SpinLock, irql);
		LOG(LHANDLE_LOGGER, LERR, "Try to open log. Log idetificators is full");
		return LHANDLE_INVALID;
	}
	Logger->NumIdentificators++;
	Logger->Identificators[(FreeIdetificator + 1) * MAX_IDENTIFICATOR_MEMORY_SIZE - 1] = 0;
	strncpy(&Logger->Identificators[FreeIdetificator * MAX_IDENTIFICATOR_MEMORY_SIZE], Name, MAX_IDENTIFICATOR_MEMORY_SIZE);

	ReleaseSpinLock(&Logger->SpinLock, irql);

	LOG(LHANDLE_LOGGER, LINF, "Log opened! Handle %u. Name: %s", (unsigned)FreeIdetificator,
		&Logger->Identificators[FreeIdetificator * MAX_IDENTIFICATOR_MEMORY_SIZE]);
	return (LHANDLE)FreeIdetificator;
}

EXPORT_FUNC void LClose(LHANDLE Handle)
{
	KIRQL irql;
	UNREFERENCED_PARAMETER(irql);
	if (Logger == NULL)
		return;

	if (!Logger->Initialized)
		return;
	if (Handle >= Logger->IdentificatorsSize || Handle == 0)
	{
		LOG(LHANDLE_LOGGER, LERR, "Try to close log. Invalid log handle %u", (unsigned)Handle);
		return;
	}

	LOG(LHANDLE_LOGGER, LINF, "Log closed! Handle %u. Name: %s", (unsigned)Handle,
		&Logger->Identificators[Handle * MAX_IDENTIFICATOR_MEMORY_SIZE]);

	AcquireSpinLock(&Logger->SpinLock, &irql);
	Logger->Identificators[Handle * MAX_IDENTIFICATOR_MEMORY_SIZE] = 0;
	Logger->NumIdentificators--;
	ReleaseSpinLock(&Logger->SpinLock, irql);
}

static void LogFull()
{
	char Str[LOG_FULL_STRING_SIZE];
	size_t Size = snprintf(Str, LOG_FULL_STRING_SIZE, LOG_FULL_FORMAT, Logger->Identificators);
	if (Size > LOG_FULL_STRING_SIZE)
		return; // log is full. just return

	RBWriteReserved(&Logger->RB, Str, Size);
}

static void GetLogLevelString(LogLevel Level, char* LevelString)
{
	if (Level >= LOG_LEVEL_NONE)
		strncpy(LevelString, "INV", 3);
	else
	{
		switch (Level)
		{
		case LDBG:
			strncpy(LevelString, "DBG", 3);
			break;
		case LINF:
			strncpy(LevelString, "INF", 3);
			break;
		case LWRN:
			strncpy(LevelString, "WRN", 3);
			break;
		case LERR:
			strncpy(LevelString, "ERR", 3);
			break;
		case LFTL:
			strncpy(LevelString, "FTL", 3);
			break;
		}
	}
}

#define MAX_FORMAT_SIZE 128
EXPORT_FUNC BOOL LPrint(LHANDLE Handle, LogLevel Level, const char* Str, size_t Size)
{

	BOOL InvalidIdentificator = FALSE;
	unsigned Time[NUM_TIME_PARAMETERS];
	char LevelString[4] = "";
	char* Identificator = "INVALID";
	char Format[MAX_FORMAT_SIZE] = "";
	size_t FormatSize;
	size_t Written;
	RBMSGHandle hndl = { 0 };
	char* NewLine = "\n";

	if (Logger == NULL)
		return FALSE;

	if (!Logger->Initialized)
		return FALSE;
	if (Handle >= Logger->IdentificatorsSize)
	{
		LOG(LHANDLE_LOGGER, LERR, "Try to write log. Invalid log handle %u", (unsigned)Handle);
		InvalidIdentificator = TRUE;
	}
	if (Level >= LOG_LEVEL_NONE)
		LOG(LHANDLE_LOGGER, LERR, "Try to write log. Invalid log level %u", (unsigned)Level);
	if (Level < Logger->Level)
		return TRUE;

	LGetTime(Time);

	GetLogLevelString(Level, LevelString);

	if ((RBFreeSize(&Logger->RB) * 100 / RBSize(&Logger->RB)) <= Logger->FlushPercent)
		LSetFlushEvent();

	if (!InvalidIdentificator)
		Identificator = &Logger->Identificators[Handle * MAX_IDENTIFICATOR_MEMORY_SIZE];
	FormatSize = snprintf(Format, MAX_FORMAT_SIZE, LOG_FORMAT, LevelString, Time[TIME_DAY], Time[TIME_HOUR], Time[TIME_YEAR],
		Time[TIME_HOUR], Time[TIME_MINUTE], Time[TIME_SECOND], Time[TIME_MILLISECONDS], Identificator);
	if (FormatSize >= MAX_FORMAT_SIZE)
		return FALSE; // log format overflow. In this case we can't call LOG. Just return

	
	if (!RBReceiveHandle(&Logger->RB, &hndl, FormatSize + Size + 2)) {
		LogFull();
		return FALSE;
	}
	Written = RBHandleWrite(&Logger->RB, &hndl, Format, FormatSize);
	Written = RBHandleWrite(&Logger->RB, &hndl, Str, Size);
	Written = RBHandleWrite(&Logger->RB, &hndl, NewLine, 2);
	RBHandleClose(&Logger->RB, &hndl);

	return TRUE;
}

EXPORT_FUNC void LFlush() {
	LSetFlushEvent();
}