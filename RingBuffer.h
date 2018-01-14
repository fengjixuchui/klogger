#ifndef __RINGBUFFER__
#define __RINGBUFFER__

#ifndef BOOL
typedef int BOOL;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#include "Main.h"

typedef struct
{
	size_t Size;
	char* Data;
	size_t Begin;
	size_t End;
	size_t ReservedBytes;

} RingBuffer;

BOOL RBInit(RingBuffer* RB, size_t Size, size_t ReservedBytes);
void RBDestroy(RingBuffer* RB);
BOOL RBEmpty(const RingBuffer* RB);
size_t RBSize(const RingBuffer* RB);
size_t RBFreeSize(const RingBuffer* RB);
const char* RBRead(RingBuffer* RB, size_t* Size);
size_t RBWrite(RingBuffer* RB, const char* Str, size_t Size);
size_t RBWriteReserved(RingBuffer* RB, const char* Str, size_t Size);

#endif // __RINGBUFFER__