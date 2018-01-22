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

#define LOG_FULL_FORMAT "\nLog is full! %" STR(MAX_IDENTIFICATOR_NAME_SIZE) "s log message was clipped\n"
#define LOG_FULL_STRING_SIZE 1024

#define LHANDLE_LOGGER ((LHANDLE) 0)
#define LOGGER_NAME "LOGGER"

LoggerStruct* Logger = NULL;

static size_t CalculateReservedBytes()
{
	char TempTag[MAX_IDENTIFICATOR_NAME_SIZE + 1] = "";
	char Temp[LOG_FULL_STRING_SIZE];
	size_t ReservedBytes;
	int i;

	for (i = 0; i < MAX_IDENTIFICATOR_NAME_SIZE; i++)
		TempTag[i] = ' ';
	ReservedBytes = snprintf(Temp, LOG_FULL_STRING_SIZE, LOG_FULL_FORMAT, TempTag);
	if (ReservedBytes >= LOG_FULL_STRING_SIZE)
		return (size_t) -1;

	return ReservedBytes + 30;
}

#ifdef _KERNEL_MODE
LErrorCode LInit(PUNICODE_STRING RegPath)
#else
LErrorCode LInit()
#endif
{
	size_t ReservedBytes;
	WCHAR* FileName;
	LInitializationParameters Parameters;
	LErrorCode Code;
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
	Logger->IdCount = MAX(Logger->IdCount, 1);
	Logger->Level = MIN(Logger->Level, LOG_LEVEL_NONE);
	Logger->FlushPercent = MAX(MIN(Logger->FlushPercent, 100), 1);
	Parameters.RingBufferSize = MAX(Parameters.RingBufferSize, MIN_RING_BUFFER_SIZE);
	
	Logger->NumIdentificators = 0;

	Logger->Identificators = MemoryAlloc(Logger->IdCount * sizeof(Identificator), pool);
	if (!Logger->Identificators) {
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_MEMORY_ALLOC;
	}
	
	memset(Logger->Identificators, 0, Logger->IdCount * sizeof(Identificator));

	Logger->pool = pool;
	memcpy(Logger->Identificators[LHANDLE_LOGGER].name, LOGGER_NAME, sizeof(LOGGER_NAME));
	if ((Logger->Identificators[LHANDLE_LOGGER].storage = MemoryAlloc(STORAGE_SIZE, pool)) == NULL) {
		MemoryFree(Logger->Identificators, Logger->IdCount * sizeof(Identificator));
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_MEMORY_ALLOC;
	}
	

	if (!RBInit(&Logger->RB, Parameters.RingBufferSize, ReservedBytes, Parameters.WaitAtPassive, pool))
	{
		MemoryFree(Logger->Identificators[LHANDLE_LOGGER].storage, STORAGE_SIZE);
		MemoryFree(Logger->Identificators, Logger->IdCount * sizeof(Identificator));
		Logger = NULL;
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return LERROR_MEMORY_ALLOC;
	}

	Code = LInitializeObjects(FileName);
	if (Code != LERROR_SUCCESS)
	{
		MemoryFree(Logger->Identificators[LHANDLE_LOGGER].storage, STORAGE_SIZE);
		MemoryFree(Logger->Identificators, Logger->IdCount * sizeof(Identificator));
		Logger = NULL;
		RBDestroy(&(logger_try->RB));
		MemoryFree(logger_try, sizeof(LoggerStruct));
		MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));
		return Code;
	}

	Logger->Initialized = TRUE;
	LOG(LHANDLE_LOGGER, LINF, "Log inited\nFilename: %ws\nFlush: %u%%\nNum identificators: %d\nLevel: %d\nOutput dbg: %s\nTimeout: %dms\n"
		"Ring buffer size: %u\nWait at passive: %s\nMemory pool type: %s", 
		FileName, Logger->FlushPercent, (int)Logger->IdCount - 1, (int)Logger->Level, 
		Logger->OutputDbg ? "YES" : "NO", Logger->Timeout, (unsigned int) Parameters.RingBufferSize, 
		Parameters.WaitAtPassive ? "YES" : "NO", Parameters.NonPagedPool ? "NonPaged" : "Paged");
	MemoryFree(FileName, MAX_LOG_FILENAME_SIZE * sizeof(WCHAR));

	return LERROR_SUCCESS;
}

void LDestroy()  //Library user responsibility not to call before everyone stopped logging 
{
	int i;
	if (Logger == NULL)
		return;

	if (Logger->Initialized == FALSE)
		return;

	if (Logger->NumIdentificators != 0)
		LOG(LHANDLE_LOGGER, LWRN, "%u identificators not closed!", (int)Logger->NumIdentificators);
	
	LOG(LHANDLE_LOGGER, LINF, "Log destroyed\n");

	LDestroyObjects();
	DestroySpinLock(&Logger->SpinLock);

	for (i = 0; i < Logger->IdCount; i++) {
		if (Logger->Identificators[i].storage) {
			MemoryFree(Logger->Identificators[i].storage, STORAGE_SIZE);
			Logger->Identificators[i].storage = NULL;
		}
	}

	MemoryFree(Logger->Identificators, Logger->IdCount * sizeof(Identificator));

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
	int i;
	KIRQL irql;
	char* LHandleStorage;
	UNREFERENCED_PARAMETER(irql);
	size_t str_size;

	if (Logger == NULL)
		return LHANDLE_INVALID;

	if (!Logger->Initialized)
		return LHANDLE_INVALID;

	AcquireSpinLock(&Logger->SpinLock, &irql);

	FreeIdetificator = (size_t) -1;
	for (i = 0; i < Logger->IdCount; i++)
	{
		if (!Logger->Identificators[i].storage)
		{
			FreeIdetificator = i;
			Logger->Identificators[i].storage = (void*)1;
			break;
		}
	}

	ReleaseSpinLock(&Logger->SpinLock, irql);

	if (FreeIdetificator == (size_t) -1)
	{
		LOG(LHANDLE_LOGGER, LERR, "Try to open log. Log idetificators is full");
		return LHANDLE_INVALID;
	}

	LHandleStorage = MemoryAlloc(STORAGE_SIZE, Logger->pool);
	if (!LHandleStorage) {
		Logger->Identificators[FreeIdetificator].storage = NULL;
		return LERROR_MEMORY_ALLOC;
	}

	Logger->Identificators[FreeIdetificator].storage = LHandleStorage;
	memset(Logger->Identificators[FreeIdetificator].name, 0, ID_STR_SIZE);
	str_size = strnlen(Name, ID_STR_SIZE);
	memcpy(Logger->Identificators[FreeIdetificator].name, Name, str_size);
	Logger->Identificators[FreeIdetificator].name[ID_STR_SIZE - 1] = 0;
	Logger->NumIdentificators++;

	LOG((LHANDLE)FreeIdetificator, LINF, "Log opened! Handle %u. Name: %s", (unsigned)FreeIdetificator,
		Logger->Identificators[FreeIdetificator].name);
	return (LHANDLE)FreeIdetificator;
}

EXPORT_FUNC void LClose(LHANDLE Handle)
{
	KIRQL irql;
	UNREFERENCED_PARAMETER(irql);
	char* storage;

	if (Logger == NULL)
		return;

	if (!Logger->Initialized)
		return;
	if (Handle >= (unsigned)Logger->IdCount || Handle == 0)
	{
		LOG(LHANDLE_LOGGER, LERR, "Try to close log. Invalid log handle %u", (unsigned)Handle);
		return;
	}

	LOG(Handle, LINF, "Log closed! Handle %u. Name: %s", (unsigned)Handle,
		&(Logger->Identificators[Handle].name[0]));

	storage = Logger->Identificators[Handle].storage;
	Logger->Identificators[Handle].storage = NULL;

	MemoryFree(storage, STORAGE_SIZE);

	Logger->NumIdentificators--;

}

static void LogFull(LHANDLE hndl)
{
	size_t Size = snprintf(Logger->Identificators[hndl].storage, LOG_FULL_STRING_SIZE, LOG_FULL_FORMAT, Logger->Identificators[hndl].name);
	if (Size > LOG_FULL_STRING_SIZE)
		return; // log is full. just return

	RBWriteReserved(&Logger->RB, Logger->Identificators[hndl].storage, Size);
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

static char* IntToString(char* Str, unsigned int Number, size_t NumSymbols)
{
	for (int i = (int) NumSymbols - 1; i >= 0; i--)
	{
		Str[i] = Number % 10 + '0';
		Number /= 10;
	}
	return Str + NumSymbols;
}

#define LOG_FORMAT "    [00.00.00 00:00:00:000]           | "
#define MAX_FORMAT_SIZE sizeof(LogFormat)
EXPORT_FUNC BOOL LPrint(LHANDLE Handle, LogLevel Level, const char* Str, size_t Size)
{
	unsigned Time[NUM_TIME_PARAMETERS];
	char LevelString[4] = "";
	char* Identificator = "INVALID";
	char String[50] = LOG_FORMAT;
	char* Format = String;
	size_t FormatSize;
	size_t Written;
	RBMSGHandle hndl = { 0 };

	if (Logger == NULL)
		return FALSE;

	if (!Logger->Initialized)
		return FALSE;
	if (Handle >= (unsigned)Logger->IdCount)
	{
		if(GetIRQL() == PASSIVE_LEVEL)
			LOG(LHANDLE_LOGGER, LERR, "Try to write log. Invalid log handle %u", (unsigned)Handle);
		return FALSE;
	}

	if (Level >= LOG_LEVEL_NONE) 
	{
		if (GetIRQL() == PASSIVE_LEVEL)
			LOG(LHANDLE_LOGGER, LERR, "Try to write log. Invalid log level %u", (unsigned)Level);
		return FALSE;
	}
	if (Level < Logger->Level)
		return TRUE;

	LGetTime(Time);

	GetLogLevelString(Level, LevelString);

	
	if ((RBFreeSize(&Logger->RB) * 100 / RBSize(&Logger->RB)) <= Logger->FlushPercent)
		LSetFlushEvent();
		
	Identificator = Logger->Identificators[Handle].name;

	FormatSize = sizeof (LOG_FORMAT) - 1;

	Format = (char*) RtlCopyMemory(Format, LevelString, 3) + 3 + 2;
	Format = IntToString(Format, Time[TIME_DAY], 2) + 1;
	Format = IntToString(Format, Time[TIME_HOUR], 2) + 1;
	Format = IntToString(Format, Time[TIME_YEAR], 2) + 1;
	Format = IntToString(Format, Time[TIME_HOUR], 2) + 1;
	Format = IntToString(Format, Time[TIME_MINUTE], 2) + 1;
	Format = IntToString(Format, Time[TIME_SECOND], 2) + 1;
	Format = IntToString(Format, Time[TIME_MILLISECONDS], 3) + 2;
	RtlCopyMemory(Format, Identificator, MIN(9, strlen(Identificator)));
	
	if (!RBReceiveHandle(&Logger->RB, &hndl, FormatSize + Size + 1)) {
		LogFull(Handle);
		return FALSE;
	}
	Written = RBHandleWrite(&Logger->RB, &hndl, String, FormatSize);
	Written = RBHandleWrite(&Logger->RB, &hndl, Str, Size);
	Written = RBHandleWrite(&Logger->RB, &hndl, "\n", 1);
	RBHandleClose(&Logger->RB, &hndl);

	return TRUE;
}

EXPORT_FUNC void LFlush() {
	LSetFlushEvent();
}

EXPORT_FUNC BOOL LOG(LHANDLE Handle, LogLevel Level, const char* Format, ...) {
	va_list vl;
	size_t Size;
	va_start(vl, Format);
	char* end;
	UNREFERENCED_PARAMETER(end);

#ifndef _KERNEL_MODE
	Size = _vsnprintf(Logger->Identificators[Handle].storage, STORAGE_SIZE, Format, vl);
#else
	RtlStringCchVPrintfExA(Logger->Identificators[Handle].storage, STORAGE_SIZE, &end, NULL, 0, Format, vl);
	Size = end - Logger->Identificators[Handle].storage;
#endif
	BOOL Result = LPrint(Handle, Level, Logger->Identificators[Handle].storage, Size);

	va_end(vl);
	return Result;
}