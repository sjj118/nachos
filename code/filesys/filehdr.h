// filehdr.h 
//	Data structures for managing a disk file header.  
//
//	A file header describes where on disk to find the data in a file,
//	along with other information about the file (for instance, its
//	length, owner, etc.)
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"

#ifndef FILEHDR_H
#define FILEHDR_H

#include "disk.h"
#include "bitmap.h"
#include <ctime>

#define NumDirect 	(((SectorSize - sizeof(int) - sizeof(FileType) - 3 * sizeof(time_t)) / sizeof(int)) - 3)
#define NumIndirect (SectorSize / sizeof(int))
#define MaxFileSize 	((NumDirect + NumIndirect + NumIndirect * NumIndirect + NumIndirect * NumIndirect * NumIndirect) * SectorSize)

// The following class defines the Nachos "file header" (in UNIX terms,  
// the "i-node"), describing where on disk to find all of the data in the file.
// The file header is organized as a simple table of pointers to
// data blocks. 
//
// The file header data structure can be stored in memory or on disk.
// When it is on disk, it is stored in a single sector -- this means
// that we assume the size of this data structure to be the same
// as one disk sector.  Without indirect addressing, this
// limits the maximum file length to just under 4K bytes.
//
// There is no constructor; rather the file header can be initialized
// by allocating blocks for the file (if it is a new file), or by
// reading it from disk.

class Lock;

enum FileType { DirectoryFile, NormalFile };

class FileHeader {
  public:
    FileHeader();
    ~FileHeader();
    bool Allocate(BitMap *bitMap, int fileSize, FileType fileType);// Initialize a file header, 
						//  including allocating space 
						//  on disk for the file data
    bool ExpandSize(BitMap *bitMap, int fileSize);
    void Deallocate(BitMap *bitMap);  		// De-allocate this file's 
						//  data blocks

    void FetchFrom(int sectorNumber); 	// Initialize file header from disk
    void WriteBack(int sectorNumber); 	// Write modifications to file header
					//  back to disk

    int ByteToSector(int offset);	// Convert a byte offset into the file
					// to the disk sector containing
					// the byte

    int FileLength();			// Return the length of the file 
					// in bytes

    void Print();			// Print the contents of the file.

    void UpdateVisitedTime();
    void UpdateModifiedTime();
    FileType GetFileType() {return fileType;}

  private:
    int numBytes;			// Number of bytes in the file
    FileType fileType;            // 文件类型
    time_t createdTime;           // 创建时间
    time_t lastVisitedTime;       // 上次访问时间
    time_t lastModifiedTime;      // 上次修改时间
    int dataSectors[NumDirect + 3];		// Disk sector numbers for each data block in the file
    int numSectors;			// Number of data sectors in the file
    char *path;                   // 路径，仅存储在内存中
  public:
    Lock *lock;
    int refcount=0;
};

#endif // FILEHDR_H
