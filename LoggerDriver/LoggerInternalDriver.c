#include "../LoggerInternal.h"
#include "../Logger.h"

size_t LInitializeParameters(char* FileName)
{
	size_t Size = 4096;
	strncpy(FileName, "C:\\Users\\Jeka\\Desktop\\Log.txt", MAX_LOG_FILENAME_SIZE);

	Logger.FileLevel = LDBG;
	Logger.OutputLevel = LDBG;
	Logger.IdentificatorsSize = 10;
	Logger.Timeout = 10 * 1000;
	Logger.FlushPercent = 90;

	return Size;
}

LErrorCode LInitializeObjects(char* FileName)
{
	UNREFERENCED_PARAMETER(FileName);
	return LERROR_SUCCESS;
}

void LDestroyObjects()
{
}

void LSpinlockLock()
{
}

void LSpinlockFree()
{
}

void LSetFlushEvent()
{
}

void LGetTime(unsigned Time[NUM_TIME_PARAMETERS])
{
	UNREFERENCED_PARAMETER(Time);
}