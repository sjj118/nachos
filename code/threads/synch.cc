// synch.cc 
//	Routines for synchronizing threads.  Three kinds of
//	synchronization routines are defined here: semaphores, locks 
//   	and condition variables (the implementation of the last two
//	are left to the reader).
//
// Any implementation of a synchronization routine needs some
// primitive atomic operation.  We assume Nachos is running on
// a uniprocessor, and thus atomicity can be provided by
// turning off interrupts.  While interrupts are disabled, no
// context switch can occur, and thus the current thread is guaranteed
// to hold the CPU throughout, until interrupts are reenabled.
//
// Because some of these routines might be called with interrupts
// already disabled (Semaphore::V for one), instead of turning
// on interrupts at the end of the atomic operation, we always simply
// re-set the interrupt state back to its original value (whether
// that be disabled or enabled).
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	Initialize a semaphore, so that it can be used for synchronization.
//
//	"debugName" is an arbitrary name, useful for debugging.
//	"initialValue" is the initial value of the semaphore.
//----------------------------------------------------------------------

Semaphore::Semaphore(char* debugName, int initialValue)
{
    name = debugName;
    value = initialValue;
    queue = new List;
}

//----------------------------------------------------------------------
// Semaphore::Semaphore
// 	De-allocate semaphore, when no longer needed.  Assume no one
//	is still waiting on the semaphore!
//----------------------------------------------------------------------

Semaphore::~Semaphore()
{
    delete queue;
}

//----------------------------------------------------------------------
// Semaphore::P
// 	Wait until semaphore value > 0, then decrement.  Checking the
//	value and decrementing must be done atomically, so we
//	need to disable interrupts before checking the value.
//
//	Note that Thread::Sleep assumes that interrupts are disabled
//	when it is called.
//----------------------------------------------------------------------

void
Semaphore::P()
{
    IntStatus oldLevel = interrupt->SetLevel(IntOff);	// disable interrupts
    while (value == 0) { 			// semaphore not available
	queue->Append((void *)currentThread);	// so go to sleep
	currentThread->Sleep();
    } 
    value--; 					// semaphore available, 
						// consume its value
    
    (void) interrupt->SetLevel(oldLevel);	// re-enable interrupts
}

//----------------------------------------------------------------------
// Semaphore::V
// 	Increment semaphore value, waking up a waiter if necessary.
//	As with P(), this operation must be atomic, so we need to disable
//	interrupts.  Scheduler::ReadyToRun() assumes that threads
//	are disabled when it is called.
//----------------------------------------------------------------------

void
Semaphore::V()
{
    Thread *thread;
    IntStatus oldLevel = interrupt->SetLevel(IntOff);

    thread = (Thread *)queue->Remove();
    if (thread != NULL)	   // make thread ready, consuming the V immediately
	scheduler->ReadyToRun(thread);
    value++;
    (void) interrupt->SetLevel(oldLevel);
}

// Dummy functions -- so we can compile our later assignments 
// Note -- without a correct implementation of Condition::Wait(), 
// the test case in the network assignment won't work!
Lock::Lock(char* debugName): held(NULL), name(debugName), queue(new List){}
Lock::~Lock() {delete queue;}
void Lock::Acquire() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    while(held != NULL){
        queue->Append((void *)currentThread);
	    currentThread->Sleep();
    }
    held = currentThread;
    (void) interrupt->SetLevel(oldLevel);
}
void Lock::Release() {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(isHeldByCurrentThread());
    held = NULL;
    Thread *thread = (Thread *)queue->Remove();
    if(thread != NULL)scheduler->ReadyToRun(thread);
    (void) interrupt->SetLevel(oldLevel);
}
bool Lock::isHeldByCurrentThread() {
    return held == currentThread;
}

Condition::Condition(char* debugName): name(debugName), queue(new List) {}
Condition::~Condition() {delete queue;}
void Condition::Wait(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    conditionLock->Release();
    queue->Append((void *)currentThread);
    currentThread->Sleep();
    conditionLock->Acquire();
    (void) interrupt->SetLevel(oldLevel);
}
void Condition::Signal(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    Thread *thread = (Thread *)queue->Remove();
    if(thread != NULL)scheduler->ReadyToRun(thread);
    (void) interrupt->SetLevel(oldLevel);
}
void Condition::Broadcast(Lock* conditionLock) {
    IntStatus oldLevel = interrupt->SetLevel(IntOff);
    ASSERT(conditionLock->isHeldByCurrentThread());
    while(!queue->IsEmpty()) scheduler->ReadyToRun((Thread *)queue->Remove());
    (void) interrupt->SetLevel(oldLevel);
}

Barrier::Barrier(char* debugName, int threadNum): name(debugName), threadNum(threadNum), lock(new Lock(debugName)), cond(new Condition(debugName)){}
Barrier::~Barrier(){delete lock; delete cond;}
void Barrier::barrier(){
    lock->Acquire();
    if(--threadNum==0)cond->Broadcast(lock);
    else cond->Wait(lock);
    lock->Release();
}

ReadWriteLock::ReadWriteLock(char* debugName): name(debugName), lock(new Semaphore(debugName, 1)), mutex(new Lock(debugName)) {}
ReadWriteLock::~ReadWriteLock() {delete lock; delete mutex;}
void ReadWriteLock::AcquireRead(){
    mutex->Acquire();
    if(readers++==0)lock->P();
    mutex->Release();
}
void ReadWriteLock::ReleaseRead(){
    mutex->Acquire();
    if(--readers==0)lock->V();
    mutex->Release();
}
void ReadWriteLock::AcquireWrite(){
    lock->P();
}
void ReadWriteLock::ReleaseWrite(){
    lock->V();
}