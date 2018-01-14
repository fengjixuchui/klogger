#ifndef __MAIN__
#define __MAIN__

#ifdef _KERNEL_MODE

#include <ntddk.h>
#define MemoryAlloc(Size) ExAllocatePoolWithTag(PagedPool, Size, 'LOGG')
#define MemoryFree(Pointer,Size) ExFreePoolWithTag(Pointer, 'LOGG')

#else

#pragma warning(disable: 4996)
#include <stdlib.h>
#define MemoryAlloc(Size) malloc(Size)
#define MemoryFree(Pointer,Size) free(Pointer)

#endif

#endif __MAIN__