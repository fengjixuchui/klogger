#include <string.h>
#include "RingBuffer.h"

#ifdef _KERNEL_MODE
#define GetIRQL() KeGetCurrentIrql()
#define SpinLockObject KSPIN_LOCK

//Completely not done

#define InitSpinLock(spinlock)				KeInitializeSpinLock(spinlock)
#define AcquireSpinLock(spinlock, irql)		EnterCriticalSection(spinlock)
#define ReleaseSpinLock(spinlock, irql)		LeaveCriticalSection(spinlock)
#define DestroySpinLock(spinlock)			DeleteCriticalSection(spinlock)


#else

#include "windows.h"

#define SpinLockObject CRITICAL_SECTION
#define InitSpinLock(spinlock)				InitializeCriticalSection(spinlock)
#define AcquireSpinLock(spinlock, irql)		EnterCriticalSection(spinlock)
#define ReleaseSpinLock(spinlock, irql)		LeaveCriticalSection(spinlock)
#define DestroySpinLock(spinlock)			DeleteCriticalSection(spinlock)

#define PASSIVE_LEVEL 0
#define GetIRQL() 0
#define KIRQL char

#endif	


struct RingBuffer_
{
	size_t Size;
	size_t head;
	size_t tail;
	char* Data;

	SpinLockObject spinlock;

	size_t carry_symbols;
	char wait_at_passive;

};

typedef struct {
	size_t size;
	int written;
} RBHeader;

struct RBMSGHandle_ {
	void* current_ptr;
	RBHeader* msg_header;
	size_t symb_left;
};

RBHeader* RBGetBuffer(RingBuffer* RB, size_t size);
void* RBWriteFrom(RingBuffer* RB, const char* Str, size_t size, char* start);



BOOL RBInit(RingBuffer* RB, size_t Size, size_t ReservedBytes, char wait_at_passive)
{
	if (!RB)
		return FALSE;
	if (Size <= ReservedBytes || Size == 0)
		return FALSE;
	
	RB->Size = Size;
	RB->head = 0;
	RB->tail = 0;
	RB->wait_at_passive = wait_at_passive;
	RB->carry_symbols = 0;

	RB->Data = MemoryAlloc(Size);
	if (!RB->Data)
		return FALSE;

	memset(RB->Data, 0, sizeof(RBHeader)); //writing first zero header
	InitSpinLock(&RB->spinlock);

	return TRUE;
}

void RBDestroy(RingBuffer* RB)
{
	if (!RB)
		return;

	MemoryFree(RB->Data, RB->Size);
	RB->Data = NULL;
	DestroySpinLock(&RB->spinlock);
}


size_t RBSize(const RingBuffer* RB)
{
	if (!RB)
		return 0;

	return RB->Size;
}


const char* RBGetReadPTR(RingBuffer* RB, size_t* Size)
{
	if (!RB || !Size)
		return 0;

	if (RB->carry_symbols) {
		*Size = RB->carry_symbols;
		RB->carry_symbols = 0;

		return RB->Data;
	}

	RBHeader* hdr = RB->Data + RB->tail;
	char* msg = (char*)hdr + sizeof(RBHeader);

	if ((RB->tail + hdr->size) > RB->Size) {
		RB->carry_symbols = hdr->size - (RB->Size - RB->tail);
		*Size = hdr->size - RB->carry_symbols;
	}
	else
		*Size = hdr->size;
	
	*Size = *Size - sizeof(RBHeader);
	return msg;
}

void RBRelease(RingBuffer* RB, size_t size) {
	RB->tail = (RB->tail + size) % RB->Size;
}

static void* RBWriteFrom(RingBuffer* RB, const char* Str, size_t size, char* start)
{
	char* end;

	if (size > (RB->Data + RB->Size - start)) {
		int length = RB->Data + RB->Size - start;
		memcpy(start, Str, length);
		memcpy(RB->Data, Str + length, size - length);
		end = RB->Data + size - length;
	}
	else
	{
		memcpy(start, Str, size);
		end = start + size;
	}

	return end;
}



RBMSGHandle* RBReceiveHandle(RingBuffer* RB, size_t size) {
	
	if (!RB || !size)
		return 0;

	size += sizeof(RBMSGHandle);


	if (size > RB->Size)
		return 0;

	RBHeader* hdr = RBGetBuffer(RB, size, -1);

	RBMSGHandle* handle = (RBMSGHandle*)MemoryAlloc(sizeof(RBMSGHandle));
	memset(handle, 0, sizeof(RBMSGHandle));
		
	handle->current_ptr = hdr + sizeof(RBHeader);
	handle->msg_header = hdr;
	handle->symb_left = size;

	return handle;
}


RBHeader* RBGetBuffer(RingBuffer* RB, size_t size) {  
	
	KIRQL irql;

	while (TRUE) {  
		
		//Infinitely waiting for free space (not sure if its wise)


		size_t reqired_space = size + sizeof(RBHeader);  //2x header - HDR_MSG_NEXTHDR
		size_t free_space = (RB->head > RB->tail) ? RB->head - RB->tail : RB->head + RB->Size - RB->tail;

		if (reqired_space > free_space) {
			if (RB->wait_at_passive && (GetIRQL() == PASSIVE_LEVEL))
				continue;

			return 0;
		}
			
		//spinlock asquire
		AcquireSpinLock(&RB->spinlock, &irql);

		free_space = (RB->head > RB->tail) ? RB->head - RB->tail : RB->head + RB->Size - RB->tail;

		if (reqired_space > free_space) {
			ReleaseSpinLock(&RB->spinlock, &irql);
			//release spinlock

			if (RB->wait_at_passive && (GetIRQL() == PASSIVE_LEVEL))
				continue;
				
			return 0;
		}
		
		break;
	}
		

	RBHeader* hdr = (RBHeader*)(RB->Data + RB->head);
	hdr->size = size;
	RB->head = (RB->head + size) % RB->Size;

	RBHeader* next_hdr = (RBHeader*)(RB->Data + RB->head);
	hdr->written = 0;			//Assuring reader will never encounter trash after msg, instead he will see header with not written message
	hdr->size = 0;

	ReleaseSpinLock(&RB->spinlock, &irql);

	return hdr;
}


size_t RBWrite(RingBuffer* RB, const char* str, size_t size) {

	if (!RB || !str)
		return 0;

	RBHeader* hdr = RBGetBuffer(RB, size);
	RBWriteFrom(RB, str, size, (char*)hdr + sizeof(RBHeader));
	hdr->written = 1;

	return size;
}


size_t RBHandleWrite(RingBuffer* RB, RBMSGHandle* handle, const char* str, size_t size) {
	
	if (!RB || handle)
		return 0;

	if (size > handle->symb_left)
		return 0;

	handle->current_ptr = _RBWriteFrom(RB, str, size, handle->current_ptr);
	handle->symb_left = handle->symb_left - size;

	return size;
}

void RBHandleClose(RBMSGHandle* handle) {
	handle->msg_header->written = 1;

	MemoryFree(handle);
	return;
}