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

struct RingBuffer_
{
	size_t Size;
	size_t head;
	size_t tail;
	char* Data;

	SpinLockObject* spinlock;

	size_t carry_symbols;
	POOL_TYPE pool;
	char wait_at_passive;

};

typedef struct {
	size_t size;
	int written;
} RBHeader;

struct RBMSGHandle_ {
	char* current_ptr;
	char* msg_header;
	size_t symb_left;
};


typedef struct RingBuffer_ RingBuffer;
typedef struct RBMSGHandle_ RBMSGHandle;


#define MAXTRYLIMITLESS -1

BOOL RBInit(RingBuffer* RB, size_t Size, char wait_at_passive, POOL_TYPE pool);  //to use a IRQL > 1 you need to specify non-paged pool 
void RBDestroy(RingBuffer* RB);
size_t RBSize(const RingBuffer* RB);
size_t RBFreeSize(const RingBuffer* RB);
RBMSGHandle* RBReceiveHandle(RingBuffer* RB, size_t size);
size_t RBHandleWrite(RingBuffer* RB, RBMSGHandle* handle, const char* str, size_t size);
void RBHandleClose(RingBuffer* RB, RBMSGHandle* handle);
size_t RBWrite(RingBuffer* RB, const char* str, size_t size);

char* RBGetReadPTR(RingBuffer* RB, size_t* Size);
void RBRelease(RingBuffer* RB, size_t size);


#endif // __RINGBUFFER__