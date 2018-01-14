#include <string.h>
#include "RingBuffer.h"

BOOL RBInit(RingBuffer* RB, size_t Size, size_t ReservedBytes)
{
	if (!RB)
		return FALSE;
	if (Size <= ReservedBytes || Size == 0)
		return FALSE;
	
	RB->Begin = 0;
	RB->End = 0;
	RB->ReservedBytes = ReservedBytes;
	RB->Size = Size;

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

BOOL RBEmpty(const RingBuffer* RB)
{
	if (!RB)
		return TRUE;

	return RB->Begin == RB->End;
}

size_t RBSize(const RingBuffer* RB)
{
	if (!RB)
		return 0;

	return RB->Size;
}

size_t RBFreeSize(const RingBuffer* RB)
{
	size_t FreeSize;
	if (!RB)
		return 0;

	if (RB->Begin < RB->End)
		FreeSize = (RB->Size - RB->End) + RB->Begin;
	else if (RB->Begin > RB->End)
		FreeSize = RB->Begin - RB->End;
	else
		FreeSize = RB->Size;

	return FreeSize;
}

const char* RBRead(RingBuffer* RB, size_t* Size)
{
	size_t Begin;

	if (!RB)
		return NULL;

	if (RBEmpty(RB))
		return NULL;

	if (RB->Begin >= RB->End)
	{
		Begin = RB->Begin;
		RB->Begin = 0;
		if (Size)
			*Size = RB->Size - Begin;
		return &RB->Data[Begin];
	}
	else
	{
		Begin = RB->Begin;
		RB->Begin = RB->End;
		if (Size)
			*Size = RB->End - Begin;
		if (RB->Begin == RB->Size)
			RB->Begin = 0;
		if (RB->End == RB->Size)
			RB->End = 0;
		return &RB->Data[Begin];
	}
}

static size_t RBWriteInternal(RingBuffer* RB, const char* Str, size_t Size, size_t FreeSizeNeeded)
{
	size_t FreeSize = RBFreeSize(RB);

	if (FreeSize <= FreeSizeNeeded)
		return 0;
	if (Size > FreeSize - FreeSizeNeeded)
		Size = FreeSize - FreeSizeNeeded;

	if (RB->Begin > RB->End)
	{
		memcpy(&RB->Data[RB->End], Str, Size);
		RB->End += Size;
	}
	else
	{
		memcpy(&RB->Data[RB->End], Str, RB->Size - RB->End);
		if (RB->Size - RB->End < Size)
			memcpy(RB->Data, &Str[RB->Size - RB->End], Size - (RB->Size - RB->End));
		RB->End += Size;
		if (RB->End > RB->Size)
			RB->End -= RB->Size;
	}

	return Size;
}

size_t RBWrite(RingBuffer* RB, const char* Str, size_t Size)
{
	if (!RB)
		return 0;
	return RBWriteInternal(RB, Str, Size, RB->ReservedBytes);
}

size_t RBWriteReserved(RingBuffer* RB, const char* Str, size_t Size)
{
	if (!RB)
		return 0;
	return RBWriteInternal(RB, Str, Size, 0);
}