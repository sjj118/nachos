#include "copyright.h"
#include "synchconsole.h"

static void ConsoleWriteDone (int arg){
    SynchConsole* console = (SynchConsole *)arg;
    console->WriteDone();
}

static void ConsoleReadAvail (int arg){
    SynchConsole* console = (SynchConsole *)arg;
    console->ReadAvail();
}

SynchConsole::SynchConsole(char *readFile, char *writeFile){
    readsemaphore = new Semaphore("synch console read semaphore", 0);
    writesemaphore = new Semaphore("synch console write semaphore", 0);
    readlock = new Lock("synch console read lock");
    writelock = new Lock("synch console write lock");
    console = new Console(readFile, writeFile, ConsoleReadAvail, ConsoleWriteDone, (int) this);
}

SynchConsole::~SynchConsole(){
    delete console;
    delete readlock;
    delete writelock;
    delete readsemaphore;
    delete writesemaphore;
}

void SynchConsole::PutChar(char ch){
    writelock->Acquire();
    console->PutChar(ch);
    writesemaphore->P();
    writelock->Release();
}

char SynchConsole::GetChar(){
    readlock->Acquire();
    readsemaphore->P();
    char ch = console->GetChar();
    readlock->Release();
    return ch;
}

void SynchConsole::WriteDone(){
    writesemaphore->V();
}

void SynchConsole::ReadAvail(){
    readsemaphore->V();
}