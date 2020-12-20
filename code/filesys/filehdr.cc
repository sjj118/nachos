// filehdr.cc 
//	Routines for managing the disk file header (in UNIX, this
//	would be called the i-node).
//
//	The file header is used to locate where on disk the 
//	file's data is stored.  We implement this as a fixed size
//	table of pointers -- each entry in the table points to the 
//	disk sector containing that portion of the file data
//	(in other words, there are no indirect or doubly indirect 
//	blocks). The table size is chosen so that the file header
//	will be just big enough to fit in one disk sector, 
//
//      Unlike in a real system, we do not keep track of file permissions, 
//	ownership, last modification date, etc., in the file header. 
//
//	A file header can be initialized in two ways:
//	   for a new file, by modifying the in-memory data structure
//	     to point to the newly allocated data blocks
//	   for a file already on disk, by reading the file header from disk
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "synch.h"
#include "system.h"
#include "filehdr.h"

FileHeader::FileHeader(){
    lock = new Lock("file header lock");
}

FileHeader::~FileHeader(){
    delete lock;
}


//----------------------------------------------------------------------
// FileHeader::Allocate
// 	Initialize a fresh file header for a newly created file.
//	Allocate data blocks for the file out of the map of free disk blocks.
//	Return FALSE if there are not enough free blocks to accomodate
//	the new file.
//
//	"freeMap" is the bit map of free disk sectors
//	"fileSize" is the bit map of free disk sectors
//----------------------------------------------------------------------

bool FileHeader::Allocate(BitMap *freeMap, int fileSize, FileType fileType){
    this->fileType = fileType;
    lastModifiedTime = lastVisitedTime = createdTime = time(0);
    numBytes = fileSize;
    numSectors  = divRoundUp(fileSize, SectorSize);
    int remainSectors = numSectors;
    for(int i = 0;i < NumDirect && remainSectors;i++){      // 直接索引
        dataSectors[i] = freeMap->Find();
        ASSERT(dataSectors[i] != -1);
        --remainSectors;
    }
    if(!remainSectors)return TRUE;    // 一级索引
    dataSectors[NumDirect] = freeMap->Find();
    ASSERT(dataSectors[NumDirect] != -1);
    int *buffer1 = new int[NumIndirect];
	for(int i = 0;i < NumIndirect && remainSectors;i++){
        buffer1[i] = freeMap->Find();
        ASSERT(buffer1[i] != -1);
        --remainSectors;
    }
    synchDisk->WriteSector(dataSectors[NumDirect], (char *)buffer1); 
    delete[] buffer1;
    if(!remainSectors)return TRUE;    // 二级索引
    dataSectors[NumDirect + 1] = freeMap->Find();
    ASSERT(dataSectors[NumDirect + 1] != -1);
    buffer1 = new int[NumIndirect];
    int *buffer2 = new int[NumIndirect];
    for(int i = 0;i < NumIndirect && remainSectors;i++){ 
        buffer1[i] = freeMap->Find();
        ASSERT(buffer1[i] != -1);
        for(int j = 0;j < NumIndirect && remainSectors;j++){
            buffer2[j] = freeMap->Find();
            ASSERT(buffer2[j] != -1);
            --remainSectors;
        }
        synchDisk->WriteSector(buffer1[i], (char *)buffer2); 
    }
    synchDisk->WriteSector(dataSectors[NumDirect + 1], (char *)buffer1); 
    delete[] buffer1;
    delete[] buffer2;
    if(!remainSectors)return TRUE;    // 三级索引
    dataSectors[NumDirect + 2] = freeMap->Find();
    ASSERT(dataSectors[NumDirect + 2] != -1);
    buffer1 = new int[NumIndirect];
    buffer2 = new int[NumIndirect];
    int *buffer3 = new int[NumIndirect];
    for(int i = 0;i < NumIndirect && remainSectors;i++){ 
        buffer1[i] = freeMap->Find();
        ASSERT(buffer1[i] != -1);
        for(int j = 0;j < NumIndirect && remainSectors;j++){
            buffer2[j] = freeMap->Find();
            ASSERT(buffer2[j] != -1);
            for(int k = 0;k < NumIndirect && remainSectors;k++){
                buffer3[k] = freeMap->Find();
                ASSERT(buffer3[k] != -1);
                --remainSectors;
            }
            synchDisk->WriteSector(buffer2[j], (char *)buffer3); 
        }
        synchDisk->WriteSector(buffer1[i], (char *)buffer2); 
    }
    synchDisk->WriteSector(dataSectors[NumDirect + 2], (char *)buffer1); 
    delete[] buffer1;
    delete[] buffer2;
    delete[] buffer3;
    return TRUE;
}

bool FileHeader::ExpandSize(BitMap *freeMap, int fileSize){
    lastModifiedTime = lastVisitedTime = time(0);
    int oldSectors = divRoundUp(numBytes, SectorSize);
    numBytes = fileSize;
    numSectors = divRoundUp(fileSize, SectorSize);
    int remainSectors = numSectors;
    for(int i = 0;i < NumDirect && remainSectors;i++){      // 直接索引
        if(!oldSectors){
            dataSectors[i] = freeMap->Find();
            ASSERT(dataSectors[i] != -1);
        }else --oldSectors;
        --remainSectors;
    }
    if(!remainSectors)return TRUE;    // 一级索引
    if(!oldSectors){
        dataSectors[NumDirect] = freeMap->Find();
        ASSERT(dataSectors[NumDirect] != -1);
    }
    int *buffer1 = new int[NumIndirect];
    synchDisk->ReadSector(dataSectors[NumDirect], (char *)buffer1); 
	for(int i = 0;i < NumIndirect && remainSectors;i++){
        if(!oldSectors){
            buffer1[i] = freeMap->Find();
            ASSERT(buffer1[i] != -1);
        }else --oldSectors;
        --remainSectors;
    }
    if(!oldSectors)synchDisk->WriteSector(dataSectors[NumDirect], (char *)buffer1); 
    delete[] buffer1;
    if(!remainSectors)return TRUE;    // 二级索引
    if(!oldSectors){
        dataSectors[NumDirect + 1] = freeMap->Find();
        ASSERT(dataSectors[NumDirect + 1] != -1);
    }
    buffer1 = new int[NumIndirect];
    int *buffer2 = new int[NumIndirect];
    synchDisk->ReadSector(dataSectors[NumDirect + 1], (char *)buffer1); 
    for(int i = 0;i < NumIndirect && remainSectors;i++){ 
        if(!oldSectors){
            buffer1[i] = freeMap->Find();
            ASSERT(buffer1[i] != -1);
        }
        synchDisk->ReadSector(buffer1[i], (char *)buffer2); 
        for(int j = 0;j < NumIndirect && remainSectors;j++){
            if(!oldSectors){
                buffer2[j] = freeMap->Find();
                ASSERT(buffer2[j] != -1);
            }else --oldSectors;
            --remainSectors;
        }
        if(!oldSectors)synchDisk->WriteSector(buffer1[i], (char *)buffer2); 
    }
    if(!oldSectors)synchDisk->WriteSector(dataSectors[NumDirect + 1], (char *)buffer1); 
    delete[] buffer1;
    delete[] buffer2;
    if(!remainSectors)return TRUE;    // 三级索引
    if(!oldSectors){
        dataSectors[NumDirect + 2] = freeMap->Find();
        ASSERT(dataSectors[NumDirect + 2] != -1);
    }
    buffer1 = new int[NumIndirect];
    buffer2 = new int[NumIndirect];
    int *buffer3 = new int[NumIndirect];
    synchDisk->ReadSector(dataSectors[NumDirect + 2], (char *)buffer1); 
    for(int i = 0;i < NumIndirect && remainSectors;i++){ 
        if(!oldSectors){
            buffer1[i] = freeMap->Find();
            ASSERT(buffer1[i] != -1);
        }
        synchDisk->ReadSector(buffer1[i], (char *)buffer2); 
        for(int j = 0;j < NumIndirect && remainSectors;j++){
            if(!oldSectors){
                buffer2[j] = freeMap->Find();
                ASSERT(buffer2[j] != -1);
            }
            synchDisk->ReadSector(buffer2[j], (char *)buffer3); 
            for(int k = 0;k < NumIndirect && remainSectors;k++){
                if(!oldSectors){
                    buffer3[k] = freeMap->Find();
                    ASSERT(buffer3[k] != -1);
                }else --oldSectors;
                --remainSectors;
            }
            if(!oldSectors)synchDisk->WriteSector(buffer2[j], (char *)buffer3); 
        }
        if(!oldSectors)synchDisk->WriteSector(buffer1[i], (char *)buffer2); 
    }
    if(!oldSectors)synchDisk->WriteSector(dataSectors[NumDirect + 2], (char *)buffer1); 
    delete[] buffer1;
    delete[] buffer2;
    delete[] buffer3;
    return TRUE;
}

//----------------------------------------------------------------------
// FileHeader::Deallocate
// 	De-allocate all the space allocated for data blocks for this file.
//
//	"freeMap" is the bit map of free disk sectors
//----------------------------------------------------------------------

void FileHeader::Deallocate(BitMap *freeMap){
    int remainSectors = numSectors;
    for(int i = 0;i < NumDirect && remainSectors;i++){      // 直接索引
        ASSERT(freeMap->Test(dataSectors[i]));
        freeMap->Clear(dataSectors[i]);
        --remainSectors;
    }
    if(!remainSectors)return;    // 一级索引
    int *buffer1 = new int[NumIndirect];
    ASSERT(freeMap->Test(dataSectors[NumDirect]));
    synchDisk->ReadSector(dataSectors[NumDirect], (char *)buffer1);
	for(int i = 0;i < NumIndirect && remainSectors;i++){
        ASSERT(freeMap->Test(buffer1[i]));
        freeMap->Clear(buffer1[i]);
        --remainSectors;
    }
    freeMap->Clear(dataSectors[NumDirect]);
    delete[] buffer1;
    if(!remainSectors)return;    // 二级索引
    buffer1 = new int[NumIndirect];
    int *buffer2 = new int[NumIndirect];
    ASSERT(freeMap->Test(dataSectors[NumDirect + 1]));
    synchDisk->ReadSector(dataSectors[NumDirect + 1], (char *)buffer1);
    for(int i = 0;i < NumIndirect && remainSectors;i++){ 
        ASSERT(freeMap->Test(buffer1[i]));
        synchDisk->ReadSector(buffer1[i], (char *)buffer2);
        for(int j = 0;j < NumIndirect && remainSectors;j++){
            ASSERT(freeMap->Test(buffer2[j]));
            freeMap->Clear(buffer2[j]);
            --remainSectors;
        }
        freeMap->Clear(buffer1[i]);
    }
    freeMap->Clear(dataSectors[NumDirect + 1]);
    delete[] buffer1;
    delete[] buffer2;
    if(!remainSectors)return;    // 三级索引
    buffer1 = new int[NumIndirect];
    buffer2 = new int[NumIndirect];
    int *buffer3 = new int[NumIndirect];
    ASSERT(freeMap->Test(dataSectors[NumDirect + 2]));
    synchDisk->ReadSector(dataSectors[NumDirect + 2], (char *)buffer1);
    for(int i = 0;i < NumIndirect && remainSectors;i++){ 
        ASSERT(freeMap->Test(buffer1[i]));
        synchDisk->ReadSector(buffer1[i], (char *)buffer2);
        for(int j = 0;j < NumIndirect && remainSectors;j++){
            ASSERT(freeMap->Test(buffer2[j]));
            synchDisk->ReadSector(buffer2[j], (char *)buffer3);
            for(int k = 0;k < NumIndirect && remainSectors;k++){
                ASSERT(freeMap->Test(buffer3[k]));
                freeMap->Clear(buffer3[k]);
                --remainSectors;
            }
            freeMap->Clear(buffer2[j]);
        }
        freeMap->Clear(buffer1[i]);
    }
    freeMap->Clear(dataSectors[NumDirect + 2]);
    delete[] buffer1;
    delete[] buffer2;
    delete[] buffer3;
}

//----------------------------------------------------------------------
// FileHeader::FetchFrom
// 	Fetch contents of file header from disk. 
//
//	"sector" is the disk sector containing the file header
//----------------------------------------------------------------------

void FileHeader::FetchFrom(int sector){
    synchDisk->ReadSector(sector, (char *)this);
    numSectors  = divRoundUp(numBytes, SectorSize);
}

//----------------------------------------------------------------------
// FileHeader::WriteBack
// 	Write the modified contents of the file header back to disk. 
//
//	"sector" is the disk sector to contain the file header
//----------------------------------------------------------------------

void FileHeader::WriteBack(int sector) {
    synchDisk->WriteSector(sector, (char *)this); 
}

//----------------------------------------------------------------------
// FileHeader::ByteToSector
// 	Return which disk sector is storing a particular byte within the file.
//      This is essentially a translation from a virtual address (the
//	offset in the file) to a physical address (the sector where the
//	data at the offset is stored).
//
//	"offset" is the location within the file of the byte in question
//----------------------------------------------------------------------

int FileHeader::ByteToSector(int offset){
    offset /= SectorSize;
    if(offset < NumDirect) return dataSectors[offset];    // 直接索引
    offset -= NumDirect;
    if(offset < NumIndirect){       // 一级索引
        int *buffer = new int[NumIndirect];
        synchDisk->ReadSector(dataSectors[NumDirect], (char *)buffer);
        int sector = buffer[offset];
        delete[] buffer;
        return sector;
    }
    offset -= NumIndirect;
    if(offset < NumIndirect * NumIndirect){  // 二级索引
        int *buffer = new int[NumIndirect];
        synchDisk->ReadSector(dataSectors[NumDirect + 1], (char *)buffer);
        int sector = buffer[offset / NumIndirect];
        synchDisk->ReadSector(sector, (char *)buffer);
        offset %= NumIndirect;
        sector = buffer[offset];
        delete[] buffer;
        return sector;
    }
    offset -= NumIndirect * NumIndirect;
    if(offset < NumIndirect * NumIndirect * NumIndirect){  // 三级索引
        int *buffer = new int[NumIndirect];
        synchDisk->ReadSector(dataSectors[NumDirect + 2], (char *)buffer);
        int sector = buffer[offset / NumIndirect / NumIndirect];
        synchDisk->ReadSector(sector, (char *)buffer);
        offset %= NumIndirect * NumIndirect;
        sector = buffer[offset / NumIndirect];
        synchDisk->ReadSector(sector, (char *)buffer);
        offset %= NumIndirect;
        sector = buffer[offset];
        delete[] buffer;
        return sector;
    }
    ASSERT(FALSE);      // 太长
}

//----------------------------------------------------------------------
// FileHeader::FileLength
// 	Return the number of bytes in the file.
//----------------------------------------------------------------------

int
FileHeader::FileLength()
{
    return numBytes;
}

//----------------------------------------------------------------------
// FileHeader::Print
// 	Print the contents of the file header, and the contents of all
//	the data blocks pointed to by the file header.
//----------------------------------------------------------------------

void
FileHeader::Print()
{
    int i, j, k;
    char *data = new char[SectorSize];

    printf("FileHeader contents.  File size: %d.  ", numBytes);
    printf("File type: %s.\n", fileType == DirectoryFile ? "Directory" : "Normal");
    printf("Created time: %s", asctime(localtime(&createdTime)));
    printf("Last visited time: %s", asctime(localtime(&lastVisitedTime)));
    printf("Last modified time: %s", asctime(localtime(&lastModifiedTime)));
    puts("File blocks:");
    for (i = 0; i < numSectors; i++) printf("%d ", ByteToSector(i * SectorSize));
    /*
    printf("\nFile contents:\n");
    for (i = k = 0; i < numSectors; i++) {
	    synchDisk->ReadSector(ByteToSector(i * SectorSize), data);
        for (j = 0; (j < SectorSize) && (k < numBytes); j++, k++) {
            if ('\040' <= data[j] && data[j] <= '\176')   // isprint(data[j])
                printf("%c", data[j]);
            else
                printf("\\%x", (unsigned char)data[j]);
	    }
        printf("\n"); 
    }*/
    delete [] data;
}

void FileHeader::UpdateVisitedTime(){
    lastVisitedTime = time(0);
}

void FileHeader::UpdateModifiedTime(){
    lastVisitedTime = lastModifiedTime = time(0);
}