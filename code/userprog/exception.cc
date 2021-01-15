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

#include "stdlib.h"
#include "unistd.h"
#include <sys/stat.h>
#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synch.h"

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
    static int replaced = 0;
    unsigned int ppn;
    int virtAddr = machine->registers[BadVAddrReg];
    unsigned int vpn = (unsigned) virtAddr / PageSize;
    if (vpn >= machine->pageTableSize) {
		DEBUG('a', "virtual page # %d too large for page table size %d!\n", vpn, machine->pageTableSize);
		ASSERT(FALSE);
	}
    char vmname[50];
    sprintf(vmname, "VirtualMemory%d", currentThread->space);
    OpenFile *vm = fileSystem->Open(vmname);
    if((ppn = machine->bitmap->Find()) == -1){      // physics memory full
#ifdef USE_TLB
        for(int i=0;i<TLBSize;i++)if(machine->tlb[i].valid){
            machine->pageTable[machine->tlb[i].virtualPage] = machine->tlb[i];
            machine->tlb[i].valid = FALSE;
        }
#endif
        while(true){
            if(machine->pageTable[replaced].valid){
                if(machine->pageTable[replaced].use) machine->pageTable[replaced].use = FALSE;
                else break;
            }
            if(++replaced == machine->pageTableSize) replaced = 0;
        }
        ppn = machine->pageTable[replaced].physicalPage;
        if(machine->pageTable[replaced].dirty){
            vm->WriteAt(machine->mainMemory + ppn * PageSize, PageSize, replaced * PageSize);
            machine->pageTable[replaced].dirty = FALSE;
        }
        machine->pageTable[replaced].valid = FALSE;
        if(++replaced == machine->pageTableSize) replaced = 0;
    }
    machine->pageTable[vpn].valid = TRUE;
    machine->pageTable[vpn].physicalPage = ppn;
    vm->ReadAt(machine->mainMemory + ppn * PageSize, PageSize, vpn * PageSize);
    delete vm;
}

void TLBMissHandler(){
    static int ptr = 0;
    int virtAddr = machine->registers[BadVAddrReg];
    unsigned int vpn = (unsigned) virtAddr / PageSize;
    if (vpn >= machine->pageTableSize) {
		DEBUG('a', "virtual page # %d too large for page table size %d!\n", vpn, machine->pageTableSize);
		ASSERT(FALSE);
	} else if (!machine->pageTable[vpn].valid) {
		DEBUG('a', "virtual page # %d fault!\n", vpn);
		PageFaultHandler();
	}
	TranslationEntry *entry = &machine->pageTable[vpn];
    TranslationEntry *replaced;
#ifndef TLB_LRU
    replaced = &machine->tlb[ptr++];
    if(ptr == TLBSize) ptr = 0;
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

void ExecThread(int arg){
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();
    machine->Run();
}

void ForkThread(int func_addr){
    currentThread->space->InitRegisters();
    currentThread->space->RestoreState();
    machine->WriteRegister(PCReg, func_addr);
    machine->WriteRegister(NextPCReg, func_addr + 4);
    machine->Run();
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);

    if (which == SyscallException) {
        if(type == SC_Halt){
            DEBUG('T', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
        } else if(type == SC_Exit){
            DEBUG('T', "Exit with code %d.\n", machine->ReadRegister(4));
            currentThread->Finish();
        } else if(type == SC_Exec){
            int name_addr = machine->ReadRegister(4);       // 读取文件名
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            OpenFile *executable = fileSystem->Open(name);  // 创建地址空间
            AddrSpace *space = new AddrSpace(executable);
            delete executable;
            Thread *t = newThread(name);                    // 新建线程
            t->space = space;
            t->Fork(ExecThread, 0);
            machine->WriteRegister(2, (SpaceId) space);     // 设置返回值
        } else if(type == SC_Join){
            AddrSpace *space = (AddrSpace*)machine->ReadRegister(4);
            space->lock->Acquire();
            space->condition->Wait(space->lock);
            space->lock->Release();
        } else if(type == SC_Create){
            int name_addr = machine->ReadRegister(4);
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            ASSERT(fileSystem->Create(name, 0));
            delete[] name;
        } else if(type == SC_Open){
            int name_addr = machine->ReadRegister(4);
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            machine->WriteRegister(2, (OpenFileId) fileSystem->Open(name));
            delete[] name;
        } else if(type == SC_Read){
            int buffer_addr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFile* openfile = (OpenFile*) machine->ReadRegister(6);
            char *buffer = new char[size];
            if((OpenFileId)openfile == ConsoleInput) for(int i=0;i<size;i++) buffer[i] = getchar();
            else size = openfile->Read(buffer, size);
            for(int n=0;n<size;n++){
                while(!machine->WriteMem(buffer_addr + n, 1, buffer[n]));
            }
            machine->WriteRegister(2, size);
            delete[] buffer;
        } else if(type == SC_Write){
            int buffer_addr = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFile* openfile = (OpenFile*) machine->ReadRegister(6);
            char *buffer = new char[size + 4];
            for(int n=0;n<size;n++){
                while(!machine->ReadMem(buffer_addr + n, 1, (int*)(buffer + n)));
            }
            if((OpenFileId)openfile == ConsoleOutput) for(int i=0;i<size;i++) putchar(buffer[i]);
            else openfile->Write(buffer, size);
            delete[] buffer;
        } else if(type == SC_Close){
            OpenFile* openfile = (OpenFile*) machine->ReadRegister(4);
            delete openfile;
        } else if(type == SC_Fork){
            int func_addr = machine->ReadRegister(4);
            AddrSpace *space = new AddrSpace(currentThread->space);
            Thread *t = new Thread("forked thread");
            t->space = space;
            t->Fork(ForkThread, func_addr);
        } else if(type == SC_Yield){
            currentThread->Yield();
        } else if(type == SC_Pwd){
            system("pwd");
        } else if(type == SC_Ls){
            system("ls");
        } else if(type == SC_Cd){
            int name_addr = machine->ReadRegister(4);
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            chdir(name);
        } else if(type == SC_Mkdir){
            int name_addr = machine->ReadRegister(4);
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            mkdir(name, 0777);
        } else if(type == SC_Rmdir){
            int name_addr = machine->ReadRegister(4);
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            rmdir(name);
        } else if(type == SC_Remove){
            int name_addr = machine->ReadRegister(4);
            char *name = new char[FileNameMaxLen + 4];
            for(int n=0;;n++){
                while(!machine->ReadMem(name_addr + n, 1, (int*)(name + n)));
                if(name[n] == 0)break;
            }
            fileSystem->Remove(name);
        }
        int nextPC = machine->ReadRegister(NextPCReg);
        machine->WriteRegister(PCReg, nextPC);
        machine->WriteRegister(NextPCReg, nextPC + 4);
    } else if(which == PageFaultException){
        if(machine->tlb == NULL){
            DEBUG('a', "Page Fault.\n");
            PageFaultHandler();
        } else {
            DEBUG('a', "TLB Miss.\n");
            TLBMissHandler();
        }
    } else {
        printf("Unexpected user mode exception %d %d\n", which, type);
        ASSERT(FALSE);
    }
}
