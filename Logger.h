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
	#define snprintf RtlStringCchPrintfA
#else
	#include <stdio.h>
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
	LERROR_INTERNAL,
	LERROR_REGISTRY,
	LERROR_MEMORY_ALLOC,
	LERROR_CREATE_EVENT,
	LERROR_OPEN_FILE,
	LERROR_CREATE_THREAD
} LErrorCode;

#ifdef __EXPORT_DRIVER
	#define EXPORT_FUNC __declspec (dllexport)
#else
	#define EXPORT_FUNC __declspec (dllimport)
#endif

EXPORT_FUNC LErrorCode LInit();
EXPORT_FUNC void LDestroy();
EXPORT_FUNC BOOL LIsInitialized();
EXPORT_FUNC LHANDLE LOpen(const char* Name);
EXPORT_FUNC void LClose(LHANDLE Handle);
EXPORT_FUNC BOOL LPrint(LHANDLE Handle, LogLevel Level, const char* Str, size_t Size);

#define MAX_LOG_SIZE 8192
#define LOG(Handle,Level,Format,...) \
	do \
	{ \
		char __String[MAX_LOG_SIZE]; \
		size_t __Size = snprintf(__String, MAX_LOG_SIZE, Format, __VA_ARGS__); \
		LPrint(Handle, Level, __String, __Size); \
	} while (0);

#define LASSERT(Handle,Cond) \
	do \
	{ \
		if (!(Cond)) \
		{ \
			char __String[MAX_LOG_SIZE]; \
			size_t __Size = sprintf(__String, "Assertion failed: %s\nAt %s (%s:%d)", #Cond, __FUNCTION__, __FILE__, __LINE__); \
			LPrint(Handle, LEVEL_FATAL, __String, __Size); \
			__debugbreak(); \
		} \
	} while (0);

#endif // __LOGGER__