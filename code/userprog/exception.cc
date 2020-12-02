// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------

void PageFaultHandler(){
    ASSERT(FALSE);
}

void TLBMissHandler(){
    int virtAddr = machine->registers[BadVAddrReg];
    unsigned int vpn = (unsigned) virtAddr / PageSize;
    if (vpn >= machine->pageTableSize) {
		DEBUG('a', "virtual page # %d too large for page table size %d!\n", virtAddr, machine->pageTableSize);
		ASSERT(FALSE);
	} else if (!machine->pageTable[vpn].valid) {
		DEBUG('a', "virtual page # %d too large for page table size %d!\n", virtAddr, machine->pageTableSize);
		PageFaultHandler();
	}
	TranslationEntry *entry = &machine->pageTable[vpn];
    TranslationEntry *replaced;
#ifndef TLB_LRU
    replaced = &machine->tlb[machine->ptr++];
    if(machine->ptr == TLBSize) machine->ptr = 0;
#else
    replaced = &machine->tlb[0];
    for(int i=0;i<TLBSize;i++){
        if(!machine->tlb[i].valid){
            replaced = &machine->tlb[i];
            break;
        }
        if(machine->tlb[i].time < replaced->time) replaced = &machine->tlb[i];
    }
#endif
    if(replaced->valid) machine->pageTable[replaced->virtualPage] = *replaced;
    *replaced = *entry;
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if ((which == SyscallException) && (type == SC_Halt)) {
        DEBUG('T', "TLB Miss: %d, TLB Hit: %d, Total Translate: %d, TLB Miss Rate: %.2lf%%\n", TLBMissCount, TranslateCount-TLBMissCount, TranslateCount, TLBMissCount*100.0/TranslateCount);
        DEBUG('a', "Shutdown, initiated by user program.\n");
        interrupt->Halt();
    } else if(which == PageFaultException){
        if(machine->tlb == NULL){
            PageFaultHandler();
        } else {
            TLBMissHandler();
        }
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
