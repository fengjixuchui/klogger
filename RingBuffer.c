#include <string.h>
#include "RingBuffer.h"



char* RBGetBuffer(RingBuffer* RB, size_t size);
char* RBWriteFrom(RingBuffer* RB, const char* Str, size_t size, char* start);
char* RBReadFrom(RingBuffer* RB, char* dst, size_t size, char* start);

#define READHDR(hdr, from) RBReadFrom(RB, (char*)hdr, sizeof(RBHeader), (char*)(from))
#define WRITEHDR(hdr, from) RBWriteFrom(RB, (char*)hdr, sizeof(RBHeader), (char*)(from))


BOOL RBInit(RingBuffer* RB, size_t Size, char wait_at_passive, POOL_TYPE pool)
{
	if (!RB)
		return FALSE;
	if (Size == 0)
		return FALSE;
	
	RB->Size = Size;
	RB->head = 0;
	RB->tail = 0;
	RB->wait_at_passive = wait_at_passive;
	RB->carry_symbols = 0;
	RB->pool = pool;

	RB->Data = MemoryAlloc(Size, pool);
	if (!RB->Data)
		return FALSE;
	RB->spinlock = MemoryAlloc(sizeof(SpinLockObject), pool);
	if (!RB->spinlock) {
		MemoryFree(RB->Data, Size);
		return FALSE;
	}


	memset(RB->Data, 0, sizeof(RBHeader)); //writing first zero header
	InitSpinLock(RB->spinlock);

	return TRUE;
}

void RBDestroy(RingBuffer* RB)
{
	if (!RB)
		return;

	MemoryFree(RB->Data, RB->Size);
	RB->Data = NULL;
	DestroySpinLock(RB->spinlock);
	MemoryFree(RB->spinlock, sizeof(SpinLockObject));
	RB->spinlock = NULL;
}


size_t RBSize(const RingBuffer* RB)
{
	if (!RB)
		return 0;

	return RB->Size;
}

size_t RBFreeSize(const RingBuffer* RB) {
	size_t free_space = (RB->head > RB->tail) ? RB->tail + RB->Size - RB->head : RB->tail - RB->head;
	free_space = free_space + RB->Size * (RB->head == RB->tail);
	return free_space;
}


char* RBGetReadPTR(RingBuffer* RB, size_t* Size)
{
	if (!RB || !Size)
		return 0;

	if (RB->carry_symbols) {
		*Size = RB->carry_symbols;
		RB->carry_symbols = 0;

		return RB->Data;
	}

	RBHeader hdr;
	READHDR(&hdr, RB->Data + RB->tail);
	if (hdr.written == 0)
		return 0;

	char* msg = RB->Data + RB->tail + sizeof(RBHeader);

	if ((RB->tail + hdr.size) > RB->Size) {
		RB->carry_symbols = hdr.size - (RB->Size - RB->tail);
		*Size = hdr.size - RB->carry_symbols;

		if (*Size <= sizeof(RBHeader)) {  //Header is split!
			msg = RB->Data + sizeof(RBHeader) - *Size;
			*Size = hdr.size - sizeof(RBHeader);
			RB->carry_symbols = 0;
			return msg;
		}
	}
	else
		*Size = hdr.size;
	
	*Size = *Size - sizeof(RBHeader);
	return msg;
}

void RBRelease(RingBuffer* RB, size_t size) {
	int add = (RB->carry_symbols) ? 0 : sizeof(RBHeader);
	RB->tail = (RB->tail + size + add) % RB->Size;
}

static char* RBWriteFrom(RingBuffer* RB, const char* Str, size_t size, char* start)
{
	char* end;
	size_t left_length = (size_t)(RB->Data + RB->Size - (size_t)start);

	if (size > left_length) {
		memcpy(start, Str, left_length);
		memcpy(RB->Data, Str + left_length, size - left_length);
		end = RB->Data + size - left_length;
	}
	else
	{
		memcpy(start, Str, size);
		end = start + size;
	}

	return end;
}

char* RBReadFrom(RingBuffer* RB, char* dst, size_t size, char* start) {
	char* end;
	size_t left_length = (size_t)(RB->Data + RB->Size - (size_t)start);

	if (size > left_length) {
		memcpy(dst, start, left_length);
		memcpy(dst + left_length, RB->Data, size - left_length);
		end = RB->Data + size - left_length;
	}
	else
	{
		memcpy(dst, start, size);
		end = start + size;
	}

	return end;
}


RBMSGHandle* RBReceiveHandle(RingBuffer* RB, size_t size) {
	
	if (!RB || !size)
		return 0;

	if (size > RB->Size)
		return 0;

	if (GetIRQL() > DISPATCH_LEVEL)
		return 0; //cannot use ExAllocatePoolWithTag at irql>=DPC

	if ((GetIRQL() == DISPATCH_LEVEL) && (RB->pool == PagedPool)) //cannot allocate PagedPool at irql==DPC
		return 0;

	char* hdr = RBGetBuffer(RB, size);
	if (hdr == 0)
		return 0;

	RBMSGHandle* handle = (RBMSGHandle*)MemoryAlloc(sizeof(RBMSGHandle), RB->pool);
	memset(handle, 0, sizeof(RBMSGHandle));
		
	handle->current_ptr = RB->Data + ((size_t)(hdr + sizeof(RBHeader) - (size_t)RB->Data) % RB->Size);
	handle->msg_header = hdr;
	handle->symb_left = size;

	return handle;
}


char* RBGetBuffer(RingBuffer* RB, size_t size) {  
	
	KIRQL irql;

	while (TRUE) {  
		
		//Infinitely waiting for free space at passive level with flag (not sure if its wise)


		size_t reqired_space = size + 2 * sizeof(RBHeader);  //2x header - HDR_MSG_NEXTHDR
		size_t free_space = (RB->head > RB->tail) ? RB->tail + RB->Size - RB->head : RB->tail - RB->head;
		free_space = free_space + RB->Size * (RB->head == RB->tail);

		if (reqired_space > free_space) {
			if (RB->wait_at_passive && (GetIRQL() == PASSIVE_LEVEL))
				continue;

			return 0;
		}
			
		//spinlock Acquire
		AcquireSpinLock(RB->spinlock, &irql);

		free_space = (RB->head > RB->tail) ? RB->tail + RB->Size - RB->head : RB->tail - RB->head;
		free_space = free_space + RB->Size * (RB->head == RB->tail);

		if (reqired_space > free_space) {
			ReleaseSpinLock(RB->spinlock, &irql);
			//release spinlock

			if (RB->wait_at_passive && (GetIRQL() == PASSIVE_LEVEL))
				continue;
				
			return 0;
		}
		
		break;
	}
	
	char* old_head = RB->Data + RB->head;

	RBHeader local_hdr = { 0 };
	local_hdr.size = size + sizeof(RBHeader);
	WRITEHDR(&local_hdr, old_head);

	RB->head = (RB->head + size + sizeof(RBHeader)) % RB->Size;
	
	//Assuring reader will never encounter trash after msg, instead he will see header with not written message
	local_hdr.written = 0;
	local_hdr.size = 0;
	WRITEHDR(&local_hdr, RB->Data + RB->head);

	ReleaseSpinLock(RB->spinlock, &irql);

	return old_head;
}


size_t RBWrite(RingBuffer* RB, const char* str, size_t size) {

	if (!RB || !str)
		return 0;

	RBHeader local_hdr = { 0 };
	local_hdr.size = size + sizeof(RBHeader);
	local_hdr.written = 1;

	char* hdr = RBGetBuffer(RB, size);
	if (hdr == 0)
		return 0;

	size_t data_start = ((size_t)hdr - (size_t)RB->Data + sizeof(RBHeader)) % RB->Size;
	RBWriteFrom(RB, str, size, RB->Data + data_start);
	WRITEHDR(&local_hdr, hdr);

	return size;
}


size_t RBHandleWrite(RingBuffer* RB, RBMSGHandle* handle, const char* str, size_t size) {
	
	if (!RB || !handle)
		return 0;

	if (size > handle->symb_left)
		return 0;

	handle->current_ptr = RBWriteFrom(RB, str, size, handle->current_ptr);
	handle->symb_left = handle->symb_left - size;

	return size;
}

void RBHandleClose(RingBuffer* RB, RBMSGHandle* handle) {
	if (!RB || !handle)
		return;

	RBHeader local_header;

	READHDR(&local_header, handle->msg_header);
	local_header.written = 1;
	WRITEHDR(&local_header, handle->msg_header);

	MemoryFree(handle, sizeof(RBMSGHandle));
	return;
}