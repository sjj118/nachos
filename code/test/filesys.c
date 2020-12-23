/* halt.c
 *	Simple program to test whether running a user program works.
 *	
 *	Just do a "syscall" that exit the thread.
 *
 * 	NOTE: for some reason, user programs with global data structures 
 *	sometimes haven't worked in the Nachos environment.  So be careful
 *	out there!  One option is to allocate data structures as 
 * 	automatics within a procedure, but if you do this, you have to
 *	be careful to allocate a big enough stack to hold the automatics!
 */

#include "syscall.h"

char name[] = "test";
char str[] = "hello world!";
char buffer[256];
OpenFileId id;

int main() {
    Create(name);
    id = Open(name);
    Write(str, 13, id);
    Close(id);
    id = Open(name);
    Read(buffer, 13, id);
    Close(id);
    Exit((int)buffer[0]);
}
