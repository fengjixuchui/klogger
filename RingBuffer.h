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
	size_t head;
	char* tail;
	char* Data;

} RingBuffer;

typedef struct {
	size_t size;
	int written;
} RBHeader;

typedef struct {
	void* current_ptr;
	RBHeader* msg_header;
	size_t symb_left;
} RBMSGHandle;


BOOL RBInit(RingBuffer* RB, size_t Size, size_t ReservedBytes);
void RBDestroy(RingBuffer* RB);
size_t RBSize(const RingBuffer* RB);
RBMSGHandle* RBReceiveHandle(RingBuffer* RB, size_t size);
size_t RBHandleWrite(RingBuffer* RB, RBMSGHandle* handle, const char* str, size_t size);
void RBHandleClose(RBMSGHandle* handle);
RBHeader* _RBGetBuffer(RingBuffer* RB, size_t size);
void* _RBWriteFrom(RingBuffer* RB, const char* Str, size_t size, char* start);
void* RBWrite(RingBuffer* RB, const char* str, size_t size);

const char* RBRead(RingBuffer* RB, size_t* Size);
#endif // __RINGBUFFER__