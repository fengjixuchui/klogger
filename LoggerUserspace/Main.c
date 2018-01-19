#include <Windows.h>
#include "../RingBuffer.h"
#include "../Logger.h"

void RBTest()
{
	RingBuffer RB;
	RBInit(&RB, 32, 4);

	for (;;)
	{
		RBWrite(&RB, "1234567890", 10);

		while (!RBEmpty(&RB))
		{
			size_t Size = 0;
			const char* Str = RBRead(&RB, &Size);
			printf("Out (%d): %s\n", (int)Size, Str);
		}
		system("pause");
	}

	RBDestroy(&RB);
}

int main()
{
	LErrorCode Code = LInit();
	if (Code != LERROR_SUCCESS)
	{
		printf("Failed to init log. Error %d\n", Code);
		return 0;
	}

	LHANDLE handle1 = LOpen("HANDLE1");
	LHANDLE handle2 = LOpen("HANDLE2");

	LOG(handle1, LDBG, "Debug message %d", 1);
	LOG(handle2, LDBG, "Debug message %d", 2);

	LClose(handle1);
	LClose(handle2);

	//Sleep(100000);
	LDestroy();

}