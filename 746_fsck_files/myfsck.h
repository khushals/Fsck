/*

Author: Khushal Shah
Date: 2/18/2014

A file system check utility.

Part 1: Read partition table
*/
#define DEBUG
#ifdef DEBUG
#define dbg_p(...) printf(__VA_ARGS__)
#else
#define dbg_p(...) 
#endif

#define pint
#ifdef pint
#define dbg_pi(x) printf("%d\n",x)
#else
#define dbg_pi(x)
#endif

#define MBR_PARTITIONS 4
#define EBR 1
#define PTR_START 446
#define ZERO 0
#define PTR_ENTRY 16

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<fcntl.h>
#include"ext2_fs.h"
#include"genhd.h"

static int device;
static const sector_size_bytes = 512;
static int level=0;

struct partition_table{
		
		int p_no;	//partition number
		struct partition p;
		struct partition_table* next;

};
void buildList(struct partition_table** ,int sector);
void addToList(struct partition_table** head,struct partition_table* current);
void addExtendedPartition(struct partition_table** head,struct partition_table* given);

void assign_partition(struct partition_table** head);
void printList(struct partition_table** head);
struct partition_table* get_partition(struct partition_table**, int );

void checkForExtended(struct partition_table** head);
void readSuperBlock(struct partition_table* temp);

void print_superBlock(struct ext2_super_block* s);

void print_usage(void)
{
	printf("Please enter format as follows:\n ./myfsck -p <partition number>");
	printf(" -i <disk image> \n");
	exit(0);

}

void fillArgs(int argc,char** argv,int* p,char** buf)
{
	int opt;
	int partition;
    int args=0;	
	

	while(-1!=(opt=getopt(argc,argv,"p:i:f:")) || args!=2) {
	
		switch(opt)
        {
            case 'p':
     //               printf("The patition number is %d\n",atoi(optarg));
					*p = atoi(optarg);   
		   			args++;
		    		break;
            case 'i':
      //             printf("The path to image is %s\n",optarg);
            	    *buf=optarg;
		    		args++;
	            	break;
			case 'f':
					*buf=optarg;
					args++;
					break;
            default:			
	            	print_usage();

		}	
	
	if(args==2)
		break;
				

	}

}
/*
Partition -p for diskimage -i option
*/
void partitionTableEntry(int partition,int sector)
{
		struct partition_table* head=NULL;		
		struct partition_table* got=NULL;
		buildList(&head,sector);		
		assign_partition(&head);	
	//	printList(&head);	
		got=get_partition(&head,partition);
		
		if(got!=NULL)
			printf("0x%02X %d %d\n",got->p.sys_ind,got->p.start_sect,got->p.nr_sects);
		else 
			printf("-1\n");

}

void assign_partition(struct partition_table** head)
{
	int occured_once=0;
	int i=1;
	struct partition_table* temp=*head;
	
	for(;temp!=NULL;temp=temp->next) {
			temp->p_no=i;
			i++;	
	}	
	
}
struct partition_table* get_partition(struct partition_table ** head,int partition)
{
	struct partition_table* temp=*head;
	
	for(;temp!=NULL;temp=temp->next)
	{
		if(temp->p_no==partition)
			return temp;
	


	}

	return NULL;


}
void printList(struct partition_table** head)
{
	struct partition_table* temp=*head;
	for(;temp!=NULL;temp=temp->next)
	{
		printf("%02x %d \n",temp->p.sys_ind,temp->p_no);
		
	}
}
void buildList(struct partition_table** head,int sector)
{
	struct partition_table* temp=NULL;
	unsigned char buf[sector_size_bytes];
	int offset=sector;
	int i=0,j=0;

	read_device(device,sector,1,buf);
	
	for(i=0;i<MBR_PARTITIONS;i++)
	{
	
		temp=malloc(sizeof(struct partition_table));
		
		memcpy(&temp->p, ((buf+PTR_START) + (i*PTR_ENTRY)), sizeof(struct partition));
		temp->p_no=(i+1);
	
//	   	printf("temp->bootindex %02x\n",temp->p.boot_ind);
//	   	printf("temp->sys_ind %02x\n",temp->p.sys_ind);
		
		temp->p.start_sect +=offset;
		addToList(head,temp);	
	}
		checkForExtended(head);
		
	if(((temp=get_partition(head,1))!=NULL))
			readSuperBlock(temp);
}
void checkForExtended(struct partition_table** head)
{
	struct partition_table* temp=*head;
	struct partition_table* temp1=*head;
	int i=0;
	int offset=0;
	int t=0;
	char buf[sector_size_bytes];
	
	
	for(i=0;i<4 ;i++,temp=temp->next) 
	{
		
		if( temp->p.sys_ind==DOS_EXTENDED_PARTITION) {			
		
		read_device(device,temp->p.start_sect,1,buf);
		offset=temp->p.start_sect;
		
		level=ZERO;
		
		while(1) {
	
			temp1=malloc(sizeof(struct partition_table));
			memcpy(&temp1->p, ((buf+PTR_START)), sizeof(struct partition));
			
			if(level==ZERO)	 {	
				temp1->p.start_sect +=offset;
				level=EBR;
			}
			else
				temp1->p.start_sect+=t;
//	   	printf("temp->bootindex %02x\n",temp->p.boot_ind);
//	   	printf("temp->sys_ind %02x\n",temp->p.sys_ind);
			addToList(head,temp1);	
		
			temp1=malloc(sizeof(struct partition_table));
		
			memcpy(&temp1->p, ((buf+PTR_START)+(PTR_ENTRY)), sizeof(struct partition));
		
		
			if(temp1->p.nr_sects==ZERO) {
					
					free(temp1);
					break;	
			}
			
 			t=temp1->p.start_sect + offset;
		
			read_device(device,t,1,buf);
			} //while	
		} //if
	}//for
		
		
}
void addToList(struct partition_table** head,struct partition_table* current)
{
	struct partition_table* temp;
	struct partition_table* prev;
	temp=*head;
	

	
	if(temp==NULL)
	{
		*head=current;
		(*head)->next=NULL;	
	}
	else {
	
		while(temp!=NULL) {
			
			prev=temp;			
			temp=temp->next;
			
		}

		prev->next=current;
		current->next=NULL;	
	
	}
}

void readSuperBlock(struct partition_table* temp)
{
	struct ext2_super_block* get=NULL;
	char buf[sector_size_bytes*4];
	dbg_p("%s: size is %lu\n",__func__,sizeof(buf));	
	get=malloc(sizeof(struct ext2_super_block));
	
	read_device(device,temp->p.start_sect,4,buf);
	
	memcpy(get,(buf+1024),sizeof(struct ext2_super_block));
	
	if(get->s_magic == EXT2_SUPER_MAGIC)
		printf("Magic number is %04x\n",get->s_magic);
	else
		printf("NO!\n");
//	printf("The inode number is %u\n",get->s_first_ino);
//	printf("THe block size is %u\n",EXT2_BLOCK_SIZE(get));
	print_superBlock(get);	
	
}
void print_superBlock(struct ext2_super_block* s)
{
	printf("------------------------- SUPERBLOCK START -------------------------\n");
	printf("Inode count-------------> %u\n",s->s_inodes_count);
	printf("Blocks Count------------> %u\n",s->s_blocks_count);
	printf("Reserved blocks count---> %u\n",s->s_r_blocks_count);
	printf("Free Blocks Count-------> %u\n",s->s_free_blocks_count);
	printf("Free inodes Count-------> %u\n",s->s_free_inodes_count);
	printf("First data block size---> %u\n",s->s_first_data_block);
	printf("Block size--------------> %u\n",EXT2_BLOCK_SIZE(s));
	printf("Fragment size-----------> %u\n",s->s_log_frag_size);
	printf("Blocks per group--------> %u\n",s->s_blocks_per_group);
	printf("Frags per group---------> %u\n",s->s_frags_per_group);
	printf("Inodes per group--------> %u\n",s->s_inodes_per_group);
	printf("1st non-reserved inode--> %u\n",s->s_first_ino);
	printf("Inode structure size----> %u\n",EXT2_INODE_SIZE(s));
	printf("------------------------- SUPERBLOCK ENDS ------------------------\n");
	

}


