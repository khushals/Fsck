/*

Author: Khushal Shah
Date: 2/18/2014

A file system check utility.

Part 1: Read partition table

*/

#include"myfsck.h"

int main(int argc,char** argv)
{
	
	char* disk_image;
	int partition;	
	int sector=0;
	
	/*Get the arguments from command line*/	
	fillArgs(argc,argv,&partition,&disk_image);		

	if( (device = open(disk_image,O_RDWR)) == -1) {
		
		perror("Could not open the device file");
		exit(-1);
	}
	/*get the partition entry*/
	partitionTableEntry(partition,sector);
	
	return 0;
}


