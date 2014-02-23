/* 

Author: Khushal Shah
Date: 2/18/2014

A file system check utility.

Part 1: Read partition table
*/
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
#define FIRST_BLOCK 
static int first_block; //GD
#define	START_SECTOR
static int start_sector; //PARTITION
#define INODES_PER_GROUP
static int inodes_per_group; //SB
#define TOTAL_INODES
static int total_inodes; //SB
#define INODE_SIZE
static int inode_size; //SB
#define BLOCK_SIZE
static unsigned int block_size; 
// n -> inode number
//offset -> disk offset to get inode number
#define BLOCK_OFFSET(x)  (x*1024)
#define DISK_OFFSET(n) (first_block + (n-1)*inode_size)
#define INODE_NUMBER(offset) ((offset-5120)/128) + 1
#define INODE_TO_SECTOR(n) (DISK_OFFSET(n))/512
#define ROUNDS_OFF(n) ( (DISK_OFFSET(n)%512==0)?(DISK_OFFSET(n)/512):0)
#define GET_OFFSET(n) (DISK_OFFSET(n)%512)

#define GET_BLOCK_GROUP(n) (n-1)/inodes_per_group
#define GET_INDEX(n) (n)%inodes_per_group
#define GET_CONT_BLOCK(index) ( (index*inode_size) / block_size)


static int device;
static const sector_size_bytes = 512;
static int level=0;
static struct ext2_super_block* super=NULL;
static struct ext2_group_desc* group_desc=NULL;
static struct partition_table* head=NULL;
static unsigned int fs_size;
static unsigned blocks_per_group;
static unsigned total_block_count;
struct partition_table{
		
		int p_no;	//partition number
		struct partition p;
		struct partition_table* next;

};
void buildList(struct partition_table** ,int sector);
void addToList(struct partition_table** head,struct partition_table* current);
void addExtendedPartition(struct partition_table** head,struct partition_table* given);
void checkFS(int f);
void startCheck(struct partition_table* temp);

void assign_partition(struct partition_table** head);
void printList(struct partition_table** head);
struct partition_table* get_partition(struct partition_table*, int );

void checkForExtended(struct partition_table** head);
void readSuperBlock(struct partition_table* temp);
void readGroupDescriptor(char* buf);
void print_superBlock(struct ext2_super_block* s);
void print_groupDescriptor(struct ext2_group_desc* gd);
struct ext2_inode* getInode(unsigned int n);


void print_usage(void)
{
	printf("Please enter format as follows:\n");
	printf("./myfsck -p <partition number> OR");
	printf("./myfsck -f <partition number>");
	printf(" AND -i <disk image>\n");
	exit(0);

}

void fillArgs(int argc,char** argv,int* p,int *f,char** buf)
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
					*f=atoi(optarg);
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
		///struct partition_table* head=NULL;		
		struct partition_table* got=NULL;
		buildList(&head,sector);		
		assign_partition(&head);	
	//	printList(&head);	
		got=get_partition(head,partition);
		
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
struct partition_table* get_partition(struct partition_table * head,int partition)
{
	struct partition_table* temp=head;
	
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
		
//	if(((temp=get_partition(*head,1))!=NULL))
//	{			
//		printf("START_SECTOR %d\n",start_sector);	
//		readSuperBlock(temp);
//	}
//	readGroupDescriptor(super);	
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
void checkFS(int f)
{
	struct partition_table* temp=NULL;
	
	buildList(&head,ZERO);		
	assign_partition(&head);	
	
	dbg_p("f-->%d\n",f);
	
	if(f<0)
		print_usage();	
	
	if(f==ZERO)
		dbg_p("Check for every ext2 partition\n");	
	else {
	 	dbg_p("Check for %d ext2 partition\n",f);
	
		if((temp=get_partition(head,f))!=NULL){
		
			if(temp->p.sys_ind == LINUX_EXT2_PARTITION)
					startCheck(temp);
			else{
				dbg_p("Not an EXT2 Partition\n");
				return;
			}	
		}	
		else{
				
			dbg_p("NO SUCH PARTITON EXISTS!\n");
			return;
		}	


	}

}
void startCheck(struct partition_table* temp)
{
		readSuperBlock(temp);



}

void readSuperBlock(struct partition_table* temp)
{
	struct ext2_inode* inode=NULL;
	char buf[sector_size_bytes*6];
	
	dbg_p("%s: BUFFER size is %lu\n",__func__,sizeof(buf));	

	/*Malloc-read-copy*/
	super=malloc(sizeof(struct ext2_super_block));
	read_device(device,temp->p.start_sect,6,buf);
	memcpy(super,(buf+1024),sizeof(struct ext2_super_block));
	
	if(super->s_magic == EXT2_SUPER_MAGIC)
		dbg_p("Magic number is %04x\n",super->s_magic);
	else{
		dbg_p("SuperBlock Magic number does not match!!\n");
		return;
	}

	//Get the required information
	
	total_inodes=super->s_inodes_count;
	total_block_count=super->s_blocks_count;
	block_size=EXT2_BLOCK_SIZE(super);
	inode_size=EXT2_INODE_SIZE(super);
	inodes_per_group=super->s_inodes_per_group;
	blocks_per_group=super->s_blocks_per_group;	
	start_sector=temp->p.start_sect;
	
	print_superBlock(super);	
	readGroupDescriptor((buf));
	
	inode=getInode(4019);
	

}


void readGroupDescriptor(char* buf)
{
	/*malloc & copy*/
	group_desc=malloc(sizeof(struct ext2_group_desc));
	memcpy(group_desc,(buf+2048),sizeof(struct ext2_group_desc));
	first_block=BLOCK_OFFSET(group_desc->bg_inode_table);
	print_groupDescriptor(group_desc);
	dbg_p("First block %d\n",first_block);

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
void print_groupDescriptor(struct ext2_group_desc* gd)
{
	printf("---------------GROUP DESC START----------------------------------------\n");
	printf("Block Bitmap-----------> %u\n",gd->bg_block_bitmap);
	printf("Inode Bitmap-----------> %u\n",gd->bg_inode_bitmap);
	printf("Inode Table------------> %u\n",gd->bg_inode_table);
	printf("Free blocks Count------> %u\n",gd->bg_free_blocks_count);
	printf("Free inode Count-------> %u\n",gd->bg_free_inodes_count);
	printf("Used Directories Count-> %u\n",gd->bg_used_dirs_count);
	printf("---------------GROUP DESC END----------------------------------------\n");

}
struct ext2_inode* getInode(unsigned int n)
{
	struct ext2_inode* current=NULL;
	char buf[sector_size_bytes*8];
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int sector=0;
	unsigned int offset;
	unsigned int block=0;
    unsigned int current_block = GET_BLOCK_GROUP(n); 	
	unsigned int add_sectors = ((current_block*blocks_per_group*2));
	
	current=malloc(sizeof(struct ext2_inode));
	add_sectors+=start_sector;
	printf("The add sector is %d\n",add_sectors);
	sector=INODE_TO_SECTOR(GET_INDEX(n));
	sector+=add_sectors;
	printf("The sector is %d\n",sector);
	//offset=BLOCK_OFFSET(5);	
	//i=INODE_NUMBER(offset);
	//printf("THe inode is %d\n",i);
	read_device(device,(sector),3,buf);
	if(ROUNDS_OFF(GET_INDEX(n))){
		offset=0;
		printf("It does\n");
	}
	else{
		offset=GET_OFFSET(n);
		printf("It doesnt offset is %d\n",offset);
	}
	memcpy(current,(buf+offset),sizeof(struct ext2_inode));
	printf("Size in bytes %u pointer to first data block %u blocks count %d\n",current->i_size,current->i_block[0],current->i_blocks);
	printf("%d\n",6/8);
#if 1 
	block=current->i_block[0];
	block=BLOCK_OFFSET(block);
	sector=block/512;
	sector+=start_sector;
	printf("The sector is %d\n",sector);
	dir=malloc(sizeof(struct ext2_dir_entry_2));
	read_device(device,sector,1,buf);
	memcpy(dir,buf,sizeof(struct ext2_dir_entry_2));
	printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
	block=dir->rec_len;		
	while(dir->inode!=0) { 
	printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
	block+=dir->rec_len;		
	memcpy(dir,(buf+block),sizeof(struct ext2_dir_entry_2));
	}
	
		
	return current;
#endif
}


