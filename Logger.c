#include <stdio.h>
#include "Main.h"
#include "Logger.h"
#include "RingBuffer.h"
#include "LoggerInternal.h"

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
EXPORT_FUNC LErrorCode LInit(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegPath)
#else
EXPORT_FUNC LErrorCode LInit()
#endif
{
	size_t ReservedBytes;
	char FileName[MAX_LOG_FILENAME_SIZE];
	LInitializationParameters Parameters;
	LErrorCode Code;
	unsigned i;
	POOL_TYPE pool = NonPagedPoolNx;

	if (Logger != NULL) {
		if (Logger->Initialized == TRUE)
			return LERROR_ALREADY_INITIALIZED;

		return LERROR_ANOTHER_THREAD_INITIALIZING;
	}

	LoggerStruct* logger_try =  MemoryAlloc(sizeof(LoggerStruct), pool);
	if (!logger_try)
		return LERROR_MEMORY_ALLOC;
	logger_try->Initialized = FALSE;

	PVOID result = InterlockedCompareExchangePointer(&Logger, logger_try, NULL);

	if (result != NULL) {
		if(Logger->Initialized == TRUE)
			return LERROR_ALREADY_INITIALIZED;

		return LERROR_ANOTHER_THREAD_INITIALIZING;
	}	

	ReservedBytes = CalculateReservedBytes();
	if (ReservedBytes == -1) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		return LERROR_INTERNAL;
	}
	InitSpinLock(&Logger->spinlock);
#ifdef _KERNEL_MODE
	Parameters = LInitializeParameters(FileName, DriverObject, RegPath);
#else
	Parameters = LInitializeParameters(FileName);
#endif
	if (!Parameters.Status) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		return LERROR_REGISTRY;
	}
	pool = Parameters.NonPagedPool ? NonPagedPoolNx : PagedPool;
	Logger->IdentificatorsSize = MAX(Logger->IdentificatorsSize, 1);
	Logger->Level = MIN(Logger->Level, LOG_LEVEL_NONE);
	Logger->FlushPercent = MAX(MIN(Logger->FlushPercent, 100), 1);
	Parameters.RingBufferSize = MIN(Parameters.RingBufferSize, MIN_RING_BUFFER_SIZE);
	
	Logger->IdentificatorsSize++; // one identificator for logger
	Logger->NumIdentificators = 1;
	Logger->Identificators = MemoryAlloc(Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE, pool);
	if (!Logger->Identificators) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		return LERROR_MEMORY_ALLOC;
	}
	for (i = 0; i < Logger->IdentificatorsSize; i++)
		Logger->Identificators[i * MAX_IDENTIFICATOR_MEMORY_SIZE] = 0;
	strncpy(Logger->Identificators, "LOGGER", MAX_IDENTIFICATOR_NAME_SIZE);

	if (!RBInit(&Logger->RB, Parameters.RingBufferSize, 1, pool))
	{
		MemoryFree(Logger->Identificators, Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE);
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		return LERROR_MEMORY_ALLOC;
	}

	Code = LInitializeObjects(FileName);
	if (Code != LERROR_SUCCESS)
	{
		MemoryFree(Logger->Identificators, Logger->IdentificatorsSize * MAX_IDENTIFICATOR_MEMORY_SIZE);
		Logger = NULL;
		RBDestroy(&(logger_try->RB));
		MemoryFree(logger_try, sizeof(LoggerStruct));
		return Code;
	}

	LOG(LHANDLE_LOGGER, LINF, "Log inited\nFilename: %s\nFlush: %u%%\nNum identificators: %d\nLevel: %d\nOutput dbg: %s\nTimeout: %dms", 
		FileName, Logger->FlushPercent, (int)Logger->IdentificatorsSize - 1, (int)Logger->Level, 
		Logger->OutputDbg ? "YES" : "NO", Logger->Timeout);

	Logger->Initialized = TRUE;

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
	LOG(LHANDLE_LOGGER, LINF, "Log destroyed");

	LDestroyObjects();
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

	AcquireSpinLock(&Logger->spinlock, &irql);

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
		ReleaseSpinLock(&Logger->spinlock, irql);
		LOG(LHANDLE_LOGGER, LERR, "Try to open log. Log idetificators is full");
		return LHANDLE_INVALID;
	}
	Logger->NumIdentificators++;
	Logger->Identificators[(FreeIdetificator + 1) * MAX_IDENTIFICATOR_MEMORY_SIZE - 1] = 0;
	strncpy(&Logger->Identificators[FreeIdetificator * MAX_IDENTIFICATOR_MEMORY_SIZE], Name, MAX_IDENTIFICATOR_MEMORY_SIZE);

	ReleaseSpinLock(&Logger->spinlock, irql);

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

	AcquireSpinLock(&Logger->spinlock, &irql);
	Logger->Identificators[Handle * MAX_IDENTIFICATOR_MEMORY_SIZE] = 0;
	Logger->NumIdentificators--;
	ReleaseSpinLock(&Logger->spinlock, irql);
}

static void LogFull()
{
	char Str[LOG_FULL_STRING_SIZE];
	size_t Size = snprintf(Str, LOG_FULL_STRING_SIZE, LOG_FULL_FORMAT, Logger->Identificators);
	if (Size > LOG_FULL_STRING_SIZE)
		return; // log is full. just return

	RBWrite(&Logger->RB, Str, Size);
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

	size_t format_len = strlen(Format);
	size_t final_size = format_len + Size + 2;
	
	RBMSGHandle* hndl = RBReceiveHandle(&Logger->RB, final_size);
	Written = RBHandleWrite(&Logger->RB, hndl, Format, format_len);
	Written = RBHandleWrite(&Logger->RB, hndl, Str, Size);
	Written = RBHandleWrite(&Logger->RB, hndl, NewLine, 2);
	RBHandleClose(&Logger->RB, hndl);

	return TRUE;
}

EXPORT_FUNC void LFlush() {
	LSetFlushEvent();
}