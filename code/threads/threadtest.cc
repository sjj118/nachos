// threadtest.cc 
//	Simple test case for the threads assignment.
//
//	Create two threads, and have them context switch
//	back and forth between themselves by calling Thread::Yield, 
//	to illustratethe inner workings of the thread system.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "synch.h"

// testnum is set in main.cc
int testnum = 1;

//----------------------------------------------------------------------
// SimpleThread
// 	Loop 5 times, yielding the CPU to another ready thread 
//	each iteration.
//
//	"which" is simply a number identifying the thread, for debugging
//	purposes.
//----------------------------------------------------------------------

void
SimpleThread(int which)
{
    int num;
    
    for (num = 0; num < 5; num++) {
	printf("*** thread %d looped %d times\n", which, num);
        currentThread->Yield();
    }
}

//----------------------------------------------------------------------
// ThreadTest1
// 	Set up a ping-pong between two threads, by forking a thread 
//	to call SimpleThread, and then calling SimpleThread ourselves.
//----------------------------------------------------------------------

void
ThreadTest1()
{
    DEBUG('t', "Entering ThreadTest1");

    Thread *t = new Thread("forked thread");

    t->Fork(SimpleThread, 1);
    SimpleThread(0);
}

//----------------------------------------------------------------------
// OutputTidUid
// 	output uid & tid
//----------------------------------------------------------------------

void OutputTidUid(int sleep)
{
    printf("uid: %3d, tid: %3d\n", currentThread->getUid(), currentThread->getTid());
    if (sleep){
        IntStatus oldLevel = interrupt->SetLevel(IntOff);
        currentThread->Sleep();
        (void) interrupt->SetLevel(oldLevel);
    }
}

//----------------------------------------------------------------------
// ThreadTest2
// 	test uid/tid
//----------------------------------------------------------------------

void
ThreadTest2()
{
    DEBUG('t', "Entering ThreadTest2\n");
    currentThread->setPriority(10);
    for (int i=1;i<60;i++){
        Thread *t = newThread("forked thread");
        t->setPriority(11);
        t->Fork(OutputTidUid, 1);
    }
    for (int i=60;i<128;i++){
        Thread *t = newThread("forked thread");
        t->setPriority(11);
        t->Fork(OutputTidUid, 0);
    }
    currentThread->setPriority(12);
    currentThread->Yield();
    currentThread->setPriority(10);
    for (int i=60;i<128;i++){
        Thread *t = newThread("forked thread");
        t->setPriority(11);
        t->Fork(OutputTidUid, 1);
    }
    currentThread->Yield();
    TS();
}

//----------------------------------------------------------------------
// ThreadTest3
//----------------------------------------------------------------------

void LoopThread(int arg){
    for(int i=0;i<10000;i++){
        if (arg<5&&i==50){
            Thread *t = newThread("loop thread");  
            t->setPriority(4-arg);
            t->Fork(LoopThread, arg+1);
        }
        printf("%d: %d %d\n",arg,currentThread->getPriority(),currentThread->getTimeSlice());
        interrupt->OneTick();
    }
}

void ThreadTest3(){
    DEBUG('t', "Entering ThreadTest3\n");
    Thread *t = newThread("loop thread");  
    t->setPriority(5);
    t->Fork(LoopThread, 0);
}

//----------------------------------------------------------------------
// ProducerComsumerTest
//----------------------------------------------------------------------
const int N = 5;
class Slot{
    void *values[N];
    int l,r;
    int count;
public:
    void insert(void *item){
        count++;
        values[r++]=item;
        if(r==N)r=0;
    }
    void *remove(){
        count--;
        void *ret=values[l++];
        if(l==N)l=0;
        return ret;
    }
    int getCount(){
        return count;
    }
}slot;
Lock *lock;
Condition *empty, *full;
Semaphore *number, *remain, *mutex;

void ProducerThread(int arg){
    for (int i=0;i<arg;i++){
        // /* condition
        lock->Acquire();
        while(slot.getCount()==N)full->Wait(lock);
        slot.insert(new int(i));
        currentThread->Print();
        printf("inserted %d\n", i);
        empty->Signal(lock);
        lock->Release();
        // */
        /* semaphore
        remain->P();
        mutex->P();
        slot.insert(new int(i));
        currentThread->Print();
        printf("inserted %d\n", i);
        mutex->V();
        number->V();
        // */
    }
}

void ConsumerThread(int arg){
    while(true){
        // /* condition
        lock->Acquire();
        while(slot.getCount()==0)empty->Wait(lock);
        int t=*(int*)slot.remove();
        currentThread->Print();
        printf("removed %d\n", t);
        full->Signal(lock);
        lock->Release();
        // */
        /* semaphore
        number->P();
        mutex->P();
        int t=*(int*)slot.remove();
        currentThread->Print();
        printf("removed %d\n", t);
        mutex->V();
        remain->V();
        // */
    }
}

void ProducerComsumerTest(){
    DEBUG('t', "Entering ProducerComsumerTest\n");
    lock = new Lock("slot lock");
    empty = new Condition("empty condition");
    full = new Condition("full condition");
    number = new Semaphore("number semaphore", 0);
    remain = new Semaphore("remain semaphore", N);
    mutex = new Semaphore("mutex semaphore", 1);
    Thread *p1 = newThread("producer1");
    Thread *c1 = newThread("consumer1");
    Thread *c2 = newThread("consumer2");
    p1->Fork(ProducerThread, 20);
    c1->Fork(ConsumerThread, 0);
    c2->Fork(ConsumerThread, 0);
}


//----------------------------------------------------------------------
// BarrierTest
//----------------------------------------------------------------------

Barrier *barrier;

void BarrierThread(int arg){
    currentThread->Print();
    puts("before");
    if(arg)barrier->barrier();
    currentThread->Print();
    puts("after");
}

void BarrierTest(){
    DEBUG('t', "Entering BarrierTest\n");
    barrier = new Barrier("barrier", 5);
    for(int i=0;i<5;i++){
        char *name = new char[20];
        sprintf(name, "barrier thread %d", i);
        Thread *t = newThread(name);
        t->Fork(BarrierThread, 1);
    }
}

//----------------------------------------------------------------------
// ReadWriteLockTest
//----------------------------------------------------------------------

ReadWriteLock *rwLock;

void ReaderThread(int arg){
    for(int i=0;i<arg;i++){
        rwLock->AcquireRead();
        currentThread->Print();
        puts("reading...");
        for(int j=0;j<10000000;j++)interrupt->OneTick();
        currentThread->Print();
        puts("done.");
        rwLock->ReleaseRead();
    }
}

void WriterThread(int arg){
    for(int i=0;i<arg;i++){
        rwLock->AcquireWrite();
        currentThread->Print();
        puts("write");
        for(int j=0;j<10000000;j++)interrupt->OneTick();
        currentThread->Print();
        puts("done.");
        rwLock->ReleaseWrite();
    }
}

void ReadWriteLockTest(){
    DEBUG('t', "Entering ReadWriteLockTest\n");
    rwLock = new ReadWriteLock("read write");
    Thread *r1 = newThread("reader1");
    Thread *r2 = newThread("reader2");
    Thread *w1 = newThread("writer1");
    Thread *w2 = newThread("writer2");
    r1->setPriority(2);
    r2->setPriority(2);
    r1->Fork(ReaderThread, 5);
    r2->Fork(ReaderThread, 5);
    w1->Fork(WriterThread, 5);
    w2->Fork(WriterThread, 5);
}

//----------------------------------------------------------------------
// ThreadTest
// 	Invoke a test routine.
//----------------------------------------------------------------------

void
ThreadTest()
{
    switch (testnum) {
    case 1:
	ThreadTest1();
	break;
    case 2:
    ThreadTest2();
    break;
    case 3:
    ThreadTest3();
    break;
    case 4:
    ProducerComsumerTest();
    break;
    case 5:
    BarrierTest();
    break;
    case 6:
    ReadWriteLockTest();
    break;
    default:
	printf("No test specified.\n");
	break;
    }
}

