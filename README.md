# KLogger
Kernel logger driver for all IRQL levels

## Project structure
VS Solution has 3 projects:
 * **LoggerUserspace** - userspace logger for testing
 * **LoggerDriver** - klogger export driver
 * **LoggerTestDriver** - driver for testing klogger

LoggerDriver also can built with DDK.  
Shared source files are in the root directory.  
`Logger.h` - main header for including in other drivers.

## API
```c
LErrorCode LInit(PUNICODE_STRING RegPath);
void LDestroy();
BOOL LIsInitialized();
LHANDLE LOpen(const char* Name);
void LClose(LHANDLE Handle);
BOOL LPrint(LHANDLE Handle, LogLevel Level, const char* Str, size_t Size);
```

## Authors
 * Nikitenko Evgeny - [jjeka](https://github.com/jjeka)
 * Andrey Zhadchenko - [azhadchenko](https://github.com/azhadchenko)

## TODOs
 * Fix problems with segfault
 * Fix problems with synchronization
 * Add LoggerTestDriver
 * Implement registry parse in `LInitializeParameters`
 * Implement `LDestroyObjects`
 * Implement LSpinlockAcquire & LSpinlockRelease
 * Use `KeInsertQueueDpc` in `LSetFlushEvent`
 * Implement `LGetTime`
