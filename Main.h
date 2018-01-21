#ifndef __MAIN__
#define __MAIN__

#ifdef _KERNEL_MODE

#include <ntddk.h>
#define MemoryAlloc(Size, Pool) ExAllocatePoolWithTag(Pool, Size, 'LOGG')
#define MemoryFree(Pointer,Size) ExFreePoolWithTag(Pointer, 'LOGG')

#define GetIRQL() KeGetCurrentIrql()

#define SpinLockObject KSPIN_LOCK

void KernelSpinLockAcquire(PKSPIN_LOCK Kspinlock, PKIRQL irql) {
	if (GetIRQL() >= DISPATCH_LEVEL)
		KeAcquireSpinLockAtDpcLevel(Kspinlock, irql);
	else
		KeAcquireSpinLock(kspinlock, irql);
}

void KernelSpinLockRelease(PKSPIN_LOCK Kspinlock, PKIRQL irql) {
	if (*irql >= DISPATCH_LEVEL)
		KeReleaseSpinLockAtDpcLevel(Kspinlock, irql);
	else
		KeReleaseSpinLock(kspinlock, irql);
}


#define InitSpinLock(spinlock)				KeInitializeSpinLock(spinlock)
#define AcquireSpinLock(spinlock, irql)		KernelSpinLockAcquire(spinlock, irql)
#define ReleaseSpinLock(spinlock, irql)		KernelSpinLockRelease(spinlock, irql)
#define DestroySpinLock(spinlock)			1
//I guess there is no Destroy func on kernel spinlocks

#else

#pragma warning(disable: 4996)
#include <stdlib.h>
#include <windows.h>

#define MemoryAlloc(Size, Pool) malloc(Size)
#define MemoryFree(Pointer,Size) free(Pointer)

#define NonPagedPoolNx 1
#define PagedPool 0
#define POOL_TYPE int

#define KIRQL char
#define PASSIVE_LEVEL 0
#define DISPATCH_LEVEL 2
#define GetIRQL() 0

#define SpinLockObject CRITICAL_SECTION
#define InitSpinLock(spinlock)				InitializeCriticalSection(spinlock)
#define AcquireSpinLock(spinlock, irql)		EnterCriticalSection(spinlock)
#define ReleaseSpinLock(spinlock, irql)		LeaveCriticalSection(spinlock)
#define DestroySpinLock(spinlock)			DeleteCriticalSection(spinlock)

#define MM_BAD_POINTER ((void*)0)

#endif

#endif __MAIN__