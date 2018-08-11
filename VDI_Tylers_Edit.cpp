/*
 * Operating Systems Project: VDI
 * Authors
 * Tyler Weatherby
 * Zachary Cornell
 * Date: 04-28-2017
 */
#include <string>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "boot.h"
#include "datatypes.h"
#include "pickyext2.h"
using namespace std;
// Tyler's attempt at a VDI Translate
u32 vdiTranslate(HeaderStuff vdiHeader, u32 map[], u32 cursor)
{
	int location = 0;
	int page = cursor / vdiHeader.blockSize;
	int offset = cursor % vdiHeader.blockSize;
	int frame = map[page];
	location = vdiHeader.offsetData + frame *(vdiHeader.blockSize) + offset;
	return location;
}
// Tyler's near complete attempt at an getinode function
// 저는 피곤습니다... 오전 1시입니다.
ext2_inode fetchInode(int file, int inode, HeaderStuff vdiHeader, u32 map[], ext2_super_block superBlock, ext2_group_desc theTable[16], u32 size)
{
	u32 group = 0;
    u32 start = 0;
    u32 inodesPerBlock;
    u32 b = 0;
	ext2_inode currentinode;
	inode -= 1;
    group = inode / superBlock.s_inodes_per_group;
    inode = inode % superBlock.s_inodes_per_group;
    inodesPerBlock = size / sizeof(ext2_inode);
    start = theTable[group].bg_inode_table;
    b = start +(inode / inodesPerBlock);
    inode = inode % inodesPerBlock;
    u32 whatgoeshere = 0;
    lseek(file, vdiTranslate(vdiHeader, map, whatgoeshere), SEEK_SET);
    read(file, &currentinode, sizeof(ext2_inode));
	return currentinode;
}
void fetchSuperBlock(int file, ext2_super_block &superBlock, HeaderStuff vdiHeader, BootSector MBR, u32 map[])
{
	// Superblock located on offsetBlocks + 1024 of blank space
    lseek(file, vdiTranslate(vdiHeader, map, (vdiHeader.offsetBlocks)+1024), SEEK_SET);
    read(file, &superBlock, sizeof(superBlock));
}
void fetchBlockGroups(int file, HeaderStuff vdiHeader, u32 map[], BootSector MBR, ext2_group_desc blockGroups[], int blockSize, u32 numBlockGroups)
{
	// Block Groups located after Superblock, so 1024 + 1024
	if (blockSize == 1024)
	{
		lseek(file, vdiTranslate(vdiHeader, map, (vdiHeader.offsetBlocks) + 2048), SEEK_SET);
	}
	else
	{
		lseek(file, vdiTranslate(vdiHeader, map, (vdiHeader.offsetBlocks)+blockSize), SEEK_SET);
	}
	read(file, blockGroups, sizeof(ext2_group_desc)*numBlockGroups);
	return;
}
int fetchMBR(int file, HeaderStuff vdiHeader, BootSector &MBR)
{
	// Do the typical lseek, read as per notes
	lseek(file, vdiHeader.offsetData, SEEK_SET);
	read(file, &MBR, sizeof(MBR));
	// Tyler's attempt at rejecting bad files
	if(MBR.magic != 43605)
	{
	 	cout << "This does not have the correct magic number" << endl;
	   	cout << "Possibly not a VDI file, please double check" << endl;
	   	return 1;
	}
	else // Discover which table index is correct
	{
	 	int counter = 0;
	   	while(MBR.partitionTable[counter].firstSector == 0)
	   	{
	   		counter += 1;
	   	}
	   	return counter; // Return the correct index for later use
	}
}
int main(int argc, char *argv[])
{
	// Below are member variables required
    char* filename;
	filename = argv[1];
    int fd;
	int diskSize = 0;
	int totalPages = 0;
	int tableIndex = 0;
    int spaceUsed = 0;
    u32 spaceUnused = 0;
    int possibleFilesDirs = 0;
    int existingFiles = 0;
    int existingDirs = 0;
    int blockSize = 0;
    // Define structs
    BootSector MBR;
    HeaderStuff vdiHeader;
    // PartitionEntry partitionEntry;
    ext2_super_block superBlock;
    // Open the file as a read only, we definitely aren't in the position to be making any changes yet.
    fd = open(filename, O_RDONLY);
    // If file exist open it. If file does not exist, declare it, and terminate
    if( fd == -1 ){
        cout << "File did not open" << endl;
        return 1;
    }
    else{
        cout << "The file opened" << endl;
    }
    // Start vdiHeader Section
    // No need to lseek, it just starts reading from the top, where we want to be.
    read(fd, &vdiHeader, sizeof(vdiHeader)); // Should read the header into the struct
    // End vdiHeader Section
    // Start MBR Section
    tableIndex = fetchMBR(fd, vdiHeader, MBR);
    // End MBR Section
    // He's the MAP!
    u32 map[vdiHeader.blocksInHDD];
    lseek(fd, vdiHeader.offsetBlocks, SEEK_SET);
    read(fd, map, 4*vdiHeader.blocksInHDD);
    // End MAP Section
    // Start superBlock Section
    fetchSuperBlock(fd, superBlock, vdiHeader, MBR, map);
    // End superBlock Section
    blockSize = 1024 << superBlock.s_log_block_size;
    int numBlockGroups = (superBlock.s_blocks_count + superBlock.s_blocks_per_group - 1) / superBlock.s_blocks_per_group;
    // Start blockGroupDescriptorTable Section
    ext2_group_desc blockGroupDescriptorTable[numBlockGroups];
    //ext2_group_desc *blockGroupDescriptorTable = (ext2_group_desc*)malloc(sizeof(ext2_group_desc)*(numBlockGroups));
    fetchBlockGroups(fd, vdiHeader, map, MBR, blockGroupDescriptorTable, blockSize, numBlockGroups);
	// End blockGroupDescriptorTable Section
    // Begin inode section
    ext2_inode someinode;
    u32 inode = 2;
    someinode = fetchInode(fd, inode, vdiHeader, map, superBlock, blockGroupDescriptorTable, blockSize);
    // End inode section
    // Begin numeric calculations for statistics
    diskSize = vdiHeader.blocksInHDD * vdiHeader.blockSize;
    totalPages = diskSize/vdiHeader.blockSize;
    u32 gdPerBlock = blockSize / sizeof(struct ext2_group_desc);
    u32 gdtBlocksUsed = (numBlockGroups + gdPerBlock - 1) / gdPerBlock;
    u32 addrPerBlock = blockSize / sizeof(u32);
    u32 inodesPerBlock = blockSize / superBlock.s_inode_size;
    // End numeric calculations for statistics
    // Start writing crap to files for personal verifcation
    // Open outputMBR for writing to txt
    ofstream outputMBR;
    outputMBR.open("outputMBR.txt");
    outputMBR << "================================================================================" << endl;
    outputMBR << "Name: " << vdiHeader.diskImage << "\n";
    outputMBR << hex << "Signature: " << vdiHeader.imageSignature << "\n";
    outputMBR << hex << "Version: " << vdiHeader.version << "\n";
    outputMBR << dec << "Header Size: " << vdiHeader.sizeOfHeader << "\n";
    outputMBR << hex << "Image Type: " << vdiHeader.imageType << "\n";
    outputMBR << "Image Flags: " << setfill('0') << setw(8) << vdiHeader.imageFlags << "\n";
    outputMBR << "Description: " << vdiHeader.imageDesc << "\n";
    outputMBR << "Map Offset: " << setfill('0') << setw(8) << vdiHeader.offsetBlocks << "\n";
    outputMBR << "Data Offset: " << setfill('0') << setw(8) << vdiHeader.offsetData << "\n";
    outputMBR << "Cylinders: " << vdiHeader.numCylinders << "\n";
    outputMBR << "Heads: " << vdiHeader.numHeads << "\n";
    outputMBR << "Sectors: " << vdiHeader.numSectors << "\n";
    outputMBR << dec << "Sector Size: " << vdiHeader.sectorsSize << "\n";
    outputMBR << dec << "Disk Size: " << diskSize << "\n";
    outputMBR << "Page Size: " << vdiHeader.blockSize << "\n";
    outputMBR << "Extra Data: " << vdiHeader.blockExtraData << "\n";
    outputMBR << dec << "Total Pages: " << totalPages << "\n";
    outputMBR << "Allocated Pages: " << "Looking for this" << "\n";
    outputMBR << "================================================================================" << endl;
    outputMBR.close();
    cout << "Output for MBR complete" << endl;
    // outMBR Closed
    // Open outputEXT for writing to txt
    ofstream outputEXT;
	outputEXT.open("outputEXT.txt");
    outputEXT << "\t\tBlocks\t\tInodes" << endl;
    outputEXT << "Total:\t\t" << superBlock.s_blocks_count << "\t\t" << superBlock.s_inodes_count << endl;
    outputEXT << "Free:\t\t" << superBlock.s_free_blocks_count << "\t\t" << superBlock.s_free_inodes_count << endl;
    outputEXT << "Reserved:\t" << superBlock.s_r_blocks_count << endl << endl;
    outputEXT << "First Data Block:\t\t" << superBlock.s_first_data_block << endl;
    outputEXT << "Block Size:\t\t\t" << superBlock.s_log_block_size << endl;
    outputEXT << "Block Groups:\t\t\t" << (superBlock.s_inodes_count / superBlock.s_inodes_per_group) << endl;
    outputEXT << "Blocks Per Group:\t\t" << superBlock.s_blocks_per_group << endl;
    outputEXT << "Inodes Per Group:\t\t"<< superBlock.s_inodes_per_group << endl;
    outputEXT << "GDT Blocks:\t\t" << endl;
    outputEXT << "Inodes Per Block:\t\t" << inodesPerBlock << endl;
    outputEXT << "Addrs Per Block:\t\t" << addrPerBlock << endl;
    int loopBlocks = 0;
    outputEXT << "Group\t" << "Block Map\t" << "INode Map\t" << "INode Table\t" << "bFree\t\t" << "iFree" << endl;
    while (loopBlocks < numBlockGroups)
    {
    	outputEXT << loopBlocks << "\t" << blockGroupDescriptorTable[loopBlocks].bg_block_bitmap << "\t\t";
    	outputEXT << blockGroupDescriptorTable[loopBlocks].bg_inode_bitmap << "\t\t";
    	outputEXT << blockGroupDescriptorTable[loopBlocks].bg_inode_table << "\t\t";
    	outputEXT << blockGroupDescriptorTable[loopBlocks].bg_free_blocks_count << "\t\t";
    	outputEXT << blockGroupDescriptorTable[loopBlocks].bg_free_inodes_count << endl;
		loopBlocks += 1;
    }
    outputMBR.close();
    cout << "Output for EXT complete" << endl;
    // outputEXT closed
    // Start section to cout project specifications
    cout << gdPerBlock << endl;
    cout << gdtBlocksUsed << endl;
    cout << "Filesystem Size in bytes: " << diskSize << endl;
    cout << "Size for files (Used and Unused): " << (spaceUnused + spaceUsed) << endl;
    cout << "Space currently used: " << spaceUsed << endl; // Amount of space currently used
    cout << "Number of possible files and Directories: " << superBlock.s_inodes_count <<  endl;
    cout << "Numer of existing files: " << existingFiles << endl; // number of exist files
    cout << "Number of existing directories: " << existingDirs << endl; // number of existing directories
    loopBlocks = 0; // Reset to zero for printing loop
    cout << "Group\t" << "Block Map\t" << "INode Map\t" << "INode Table\t" << "bFree\t\t" << "iFree" << endl;
    while (loopBlocks < numBlockGroups)
    {
    	cout << loopBlocks << "\t" << blockGroupDescriptorTable[loopBlocks].bg_block_bitmap << "\t\t";
    	cout << blockGroupDescriptorTable[loopBlocks].bg_inode_bitmap << "\t\t";
    	cout << blockGroupDescriptorTable[loopBlocks].bg_inode_table << "\t\t";
		cout << blockGroupDescriptorTable[loopBlocks].bg_free_blocks_count << "\t\t";
		cout << blockGroupDescriptorTable[loopBlocks].bg_free_inodes_count << endl;
		loopBlocks += 1;
    }
    cout << "Block Size (Bytes): " << blockSize << endl; // block size in bytes
    cout << "State of Filesystem: " << superBlock.s_state << endl; // State of the file system
    // End project specification section
    // Close VDI File
    close(fd);
    cout << "The VDI file is closed." << endl;
    return 0;
};
