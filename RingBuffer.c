#include <string.h>
#include "RingBuffer.h"

BOOL RBInit(RingBuffer* RB, size_t Size, size_t ReservedBytes)
{
	if (!RB)
		return FALSE;
	if (Size <= ReservedBytes || Size == 0)
		return FALSE;
	
	RB->Size = Size;
	RB->head = 0;
	RB->tail = 0;

	RB->Data = MemoryAlloc(Size);
	if (!RB->Data)
		return FALSE;

	return TRUE;
}

void RBDestroy(RingBuffer* RB)
{
	if (!RB)
		return;

	MemoryFree(RB->Data, RB->Size);
	RB->Data = NULL;
}


size_t RBSize(const RingBuffer* RB)
{
	if (!RB)
		return 0;

	return RB->Size;
}


const char* RBRead(RingBuffer* RB, size_t* Size)
{
	size_t Begin;


}

static void* _RBWriteFrom(RingBuffer* RB, const char* Str, size_t size, char* start)
{
	char* end;

	if (size > RB->Data + RB->Size - start) {
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

	//Check free space

	if (size > RB->Size)
		return 0;

	RBMSGHandle* handle = (RBMSGHandle*)MemoryAlloc(sizeof(RBMSGHandle));
	memset(handle, 0, sizeof(RBMSGHandle));
	
	RBHeader* hdr = _RBGetBuffer(RB, size);
	
	hdr->size = size;
	handle->current_ptr = hdr + sizeof(RBHeader);
	handle->msg_header = hdr;
	handle->symb_left = size;

	return handle;
}


RBHeader* _RBGetBuffer(RingBuffer* RB, size_t size) {  //Beware, it does not check free space
	//acquire spinlock

	RBHeader* hdr = (RBHeader*)(RB->Data + RB->head);
	hdr->written = 0;
	RB->head = (RB->head + size) % RB->Size;

	//release spinlock

	return hdr;
}


size_t RBWrite(RingBuffer* RB, const char* str, size_t size) {

	if (!RB || !str)
		return 0;

	RBHeader* hdr = _RBGetBuffer(RB, size);
	_RBWriteFrom(RB, str, size, (char*)hdr + sizeof(RBHeader));
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