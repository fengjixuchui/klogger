#ifndef __MAIN__
#define __MAIN__

#ifdef _KERNEL_MODE

#include <ntddk.h>
#define MemoryAlloc(Size, Pool) ExAllocatePoolWithTag(Pool, Size, 'LOGG')
#define MemoryFree(Pointer,Size) ExFreePoolWithTag(Pointer, 'LOGG')

#define GetIRQL() KeGetCurrentIrql()

#define SpinLockObject KSPIN_LOCK

inline void KernelSpinLockAcquire(PKSPIN_LOCK Kspinlock, PKIRQL irql) {
	if (GetIRQL() >= DISPATCH_LEVEL)
		KeAcquireSpinLockAtDpcLevel(Kspinlock);
	else
		KeAcquireSpinLock(Kspinlock, irql);
}

inline void KernelSpinLockRelease(PKSPIN_LOCK Kspinlock, KIRQL irql) {
	if (irql >= DISPATCH_LEVEL)
		KeReleaseSpinLockFromDpcLevel(Kspinlock);
	else
		KeReleaseSpinLock(Kspinlock, irql);
}

#define InitSpinLock(spinlock)				KeInitializeSpinLock(spinlock)
#define AcquireSpinLock(spinlock, irql)		KernelSpinLockAcquire(spinlock, irql)
#define ReleaseSpinLock(spinlock, irql)		KernelSpinLockRelease(spinlock, irql)
#define DestroySpinLock(spinlock)			1

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

#endif

#endif __MAIN__