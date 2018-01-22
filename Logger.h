#ifndef __LOGGER__
#define __LOGGER__

#ifndef BOOL
	typedef int BOOL;
#endif
#ifndef TRUE
	#define TRUE 1
#endif
#ifndef FALSE
	#define FALSE 0
#endif

#ifdef _KERNEL_MODE
	#include <Ntstrsafe.h>
	#define snprintf(dst,size,format,...) KernelSnprintf(dst, size, format, __VA_ARGS__)

	__inline size_t KernelSnprintf(char* Dst, size_t Size, const char* Format, ...)
	{
		va_list vl;
		char* End;
		NTSTATUS Status;
		va_start(vl, Format);
		Status = RtlStringCchVPrintfExA(Dst, Size, &End, NULL, 0, Format, vl);
		if (!NT_SUCCESS(Status))
			return 0;
		va_end(vl);
		return End - Dst;
	}

#else
	#include <stdio.h>
#endif

#ifdef __EXPORT_DRIVER
	#define EXPORT_FUNC __declspec (dllexport)
#else
	#define EXPORT_FUNC __declspec (dllimport)
#endif

typedef unsigned LHANDLE;
#define LHANDLE_INVALID ((LHANDLE) -1)

typedef enum
{
	LDBG,
	LINF,
	LWRN,
	LERR,
	LFTL,
	LOG_LEVEL_NONE
} LogLevel;

typedef enum
{
	LERROR_SUCCESS,
	LERROR_ALREADY_INITIALIZED,
	LERROR_ANOTHER_THREAD_INITIALIZING,
	LERROR_INTERNAL,
	LERROR_REGISTRY,
	LERROR_MEMORY_ALLOC,
	LERROR_CREATE_EVENT,
	LERROR_OPEN_FILE,
	LERROR_CREATE_THREAD
} LErrorCode;

#ifdef _KERNEL_MODE
EXPORT_FUNC LErrorCode LInit(PUNICODE_STRING RegPath);
#else
EXPORT_FUNC LErrorCode LInit();
#endif
EXPORT_FUNC void LDestroy();
EXPORT_FUNC BOOL LIsInitialized();
EXPORT_FUNC LHANDLE LOpen(const char* Name);
EXPORT_FUNC void LClose(LHANDLE Handle);
EXPORT_FUNC BOOL LPrint(LHANDLE Handle, LogLevel Level, const char* Str, size_t Size);
EXPORT_FUNC void LFlush();

//BEWARE ITS REDEFINED
#define STORAGE_SIZE 1024

#define LOG(ret, Handle,Level,Format,...) \
	do \
	{ \
		size_t __Size = snprintf(Logger->Identificators[Handle].storage, STORAGE_SIZE, Format, __VA_ARGS__); \
		ret = LPrint(Handle, Level, Logger->Identificators[Handle].storage, __Size); \
	} while (0);

#define LASSERT(Handle,Cond) \
	do \
	{ \
		if (!(Cond)) \
		{ \
			size_t __Size = sprintf(Logger->Identificators[Handle].storage, "Assertion failed: %s\nAt %s (%s:%d)", #Cond, __FUNCTION__, __FILE__, __LINE__); \
			LPrint(Handle, LEVEL_FATAL, Logger->Identificators[Handle].storage, __Size); \
			__debugbreak(); \
		} \
	} while (0);

#endif // __LOGGER__