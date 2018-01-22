#include <windows.h> 
#include "../RingBuffer.h"
#include "../Logger.h"
#include "../LoggerInternal.h"
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

#define THREADCOUNT 4
#define SIZE 15

DWORD WINAPI ThreadFunc(VOID)
{
	int i = 0;
	int result;
	char hndl_name[SIZE] = { 0 };
	snprintf(hndl_name, SIZE, "%d", (int)GetCurrentThreadId());

	LHANDLE handle1 = LOpen(hndl_name);

	do {
		LOG(result, handle1, LDBG, "Debug message %d", i);
		i++;
	} while (result == TRUE);
	

	return 0;
}

void RBMuthithreadTest() {

	DWORD IDThread;
	HANDLE hThread[THREADCOUNT];
	int i;

	LErrorCode Code = LInit();

	if (Code != LERROR_SUCCESS)
	{
		printf("Failed to init log. Error %d\n", Code);
		return;
	}

	for (i = 0; i < THREADCOUNT; i++)
	{
		hThread[i] = CreateThread(NULL, // default security attributes 
			0,                           // use default stack size 
			(LPTHREAD_START_ROUTINE)ThreadFunc, // thread function 
			NULL,                    // no thread function argument 
			0,                       // use default creation flags 
			&IDThread);              // returns thread identifier 

									 // Check the return value for success. 
		if (hThread[i] == NULL)
			return;
	}

	for (i = 0; i < THREADCOUNT; i++)
		WaitForSingleObject(hThread[i], INFINITE);

}


int main()
{
	/*
	int ret = 0; 
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
		LOG(ret, handle1, LDBG, "Debug message %d", i*2);
		LOG(ret, handle2, LDBG, "Debug message %d", i*2+1);
		if (ret != TRUE) {
			LFlush();
		}
	}

	LClose(handle1);
	LClose(handle2);
	LClose(handle3);

	//Sleep(100000);
	Sleep(1000);
	LFlush();
	
	
	
	//RBTest();
	
	//RBHandleTest();
	*/
	RBMuthithreadTest();
	LFlush();
	LDestroy();
	system("pause");

}