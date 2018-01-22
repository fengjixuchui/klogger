#include <Windows.h>
#include "../RingBuffer.h"
#include "../Logger.h"
#include "assert.h"

#define TEST_STR "1234567890-"
#define TEST_STR2 "0987654321"

#define FULL "LOG IS FULL!"

void RBTest()  //debugged
{
	RingBuffer RB;
	RBInit(&RB, 512, 128, 0, NonPagedPoolNx);
	char* ptr;
	size_t size;

	for (;;)
	{
		if (!RBWrite(&RB, TEST_STR, sizeof(TEST_STR))) {

			RBWriteReserved(&RB, FULL, sizeof(FULL));
			while (ptr = RBGetReadPTR(&RB, &size))
			{
				printf("Out (%d): %.*s\n", (int)size, (int)size, ptr);
				RBRelease(&RB, size);
			}
		}

		/*
		while (ptr = RBGetReadPTR(&RB, &size))
		{
			printf("Out (%d): %.*s\n", (int)size, (int)size, ptr);
			RBRelease(&RB, size);
		}*/
		system("pause");
	}

	RBDestroy(&RB);
}

void RBHandleTest()  //debugged
{
	RingBuffer RB;
	RBInit(&RB, 128, 5, 1, NonPagedPoolNx);

	for (;;)
	{
		size_t Written;
		RBMSGHandle hndl = { 0 };
		RBReceiveHandle(&RB, &hndl, sizeof(TEST_STR) + sizeof(TEST_STR2));
		Written = RBHandleWrite(&RB, &hndl, TEST_STR, sizeof(TEST_STR) - 1);
		Written = RBHandleWrite(&RB, &hndl, TEST_STR2, sizeof(TEST_STR2) - 1);
		Written = RBHandleWrite(&RB, &hndl, " ", 2);
		RBHandleClose(&RB, &hndl);

		char* ptr;
		size_t size;

		while (ptr = RBGetReadPTR(&RB, &size))
		{
			printf("Out (%d): %.*s\n", (int)size, (int)size, ptr);
			RBRelease(&RB, size);
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
	LHANDLE handle3 = LOpen("HANDLE3");

	for (int i = 0; i < 100; i++) {
		LOG(handle1, LDBG, "Debug message %d", i*2);
		LOG(handle2, LDBG, "Debug message %d", i*2+1);
	}

	LClose(handle1);
	LClose(handle2);
	LClose(handle3);

	//Sleep(100000);
	Sleep(1000);
	LFlush();
	LDestroy();
	

	system("pause");
	
	//RBTest();
	
	//RBHandleTest();
}