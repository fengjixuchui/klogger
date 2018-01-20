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

typedef struct RingBuffer_ RingBuffer;
typedef struct RBMSGHandle_ RBMSGHandle;


#define MAXTRYLIMITLESS -1

BOOL RBInit(RingBuffer* RB, size_t Size, size_t ReservedBytes, char wait_at_passive);
void RBDestroy(RingBuffer* RB);
size_t RBSize(const RingBuffer* RB);
RBMSGHandle* RBReceiveHandle(RingBuffer* RB, size_t size);
size_t RBHandleWrite(RingBuffer* RB, RBMSGHandle* handle, const char* str, size_t size);
void RBHandleClose(RBMSGHandle* handle);
size_t RBWrite(RingBuffer* RB, const char* str, size_t size);

const char* RBRead(RingBuffer* RB, size_t* Size);



#endif // __RINGBUFFER__