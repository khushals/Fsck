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
#include<inttypes.h>
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
#define BASE_OFFSET 1024
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
#define BLOCK_BITMAP
static unsigned char* block_bitmap;
#define INODE_BITMAP
static unsigned char* inode_bitmap;

// n -> inode number
//offset -> disk offset to get inode number
#define BLOCK_OFFSET(block)  (BASE_OFFSET + (block-1)*block_size)
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
static unsigned int do_copy=0;
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
struct ext2_inode* checkInode(unsigned int n,unsigned int parent);
void checkDirectories(void);
void checkUnreferenced(void);
void checkItself(struct ext2_dir_entry_2* dir,int itself);
void checkLinkCount(void);
void checkBitMap(void);
void checkParent(struct ext2_dir_entry_2* dir,int parent);
struct ext2_inode* read_inode( int inode_no);

void print_inode(struct ext2_inode* inode);
	
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
		int pass=1;
		readSuperBlock(temp);
		while(pass!=4) {
			
			switch(pass)
			{

					case 1: 
						checkDirectories();
						pass++;
						break;
					case 2:
						checkUnreferenced();
						pass++;
						break;
					case 3:
						checkLinkCount();
						pass++;
						break;
					case 4:
						checkBitMap();
						pass++;
						break;
					default:
							break;


			}

		}


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

	while(1){
	
		if(super->s_magic == EXT2_SUPER_MAGIC){
			dbg_p("Magic number is %04x\n",super->s_magic);
			break;
		}
		else{
			
			dbg_p("SuperBlock Magic number does not match!!\n");
			
			return;
		
		
		}
		
	}
	//Get the required information
	
	total_inodes=super->s_inodes_count;
	total_block_count=super->s_blocks_count;
	block_size=EXT2_BLOCK_SIZE(super);
	inode_size=EXT2_INODE_SIZE(super);
	inodes_per_group=super->s_inodes_per_group;
	blocks_per_group=super->s_blocks_per_group;	
	start_sector=temp->p.start_sect;
	
//	print_superBlock(super);	
	readGroupDescriptor((buf));
	
//	inode=checkInode(2,2);
	

}


void readGroupDescriptor(char* buf)
{
	int64_t lret;
	int64_t sector_offset;
	ssize_t ret;
	/*malloc & copy*/
	group_desc=malloc(sizeof(struct ext2_group_desc));
	memcpy(group_desc,(buf+2048),sizeof(struct ext2_group_desc));
	first_block=BLOCK_OFFSET(group_desc->bg_inode_table);
	block_bitmap=malloc(block_size);
	
	sector_offset=BLOCK_OFFSET(group_desc->bg_block_bitmap);
	
	if( (lret=lseek64(device,sector_offset,SEEK_SET))!=sector_offset) {

		fprintf(stderr,"%s Seek to position %"PRId64" failed: "
				"returned %"PRId64"\n",__func__,sector_offset,lret);
		exit(-1);
	}
	if( (ret=read(device,block_bitmap,block_size))!=block_size) {
		
		fprintf(stderr,"%s Read sector %"PRId64"  failed: "
				"returned %"PRId64"\n",__func__,sector_offset,ret);
		exit(-1);

	}
	inode_bitmap=malloc(block_size);
	
	sector_offset=BLOCK_OFFSET(group_desc->bg_inode_bitmap);
	
	if( (lret=lseek64(device,sector_offset,SEEK_SET))!=sector_offset) {

		fprintf(stderr,"%s Seek to position %"PRId64" failed: "
				"returned %"PRId64"\n",__func__,sector_offset,lret);
		exit(-1);
	}
	if( (ret=read(device,inode_bitmap,block_size))!=block_size) {
		
		fprintf(stderr,"%s Read sector %"PRId64"  failed: "
				"returned %"PRId64"\n",__func__,sector_offset,ret);
		exit(-1);

	}
	
//	print_groupDescriptor(group_desc);
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
struct ext2_inode* checkInode(unsigned int n,unsigned int p)
{
	struct ext2_inode* current=NULL;
	char buf[BASE_OFFSET*8];
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int sector=0;
	unsigned int dsector=0;
	unsigned int offset;
	unsigned int block=0;
    unsigned int dots=0;
	unsigned int current_block = GET_BLOCK_GROUP(n); 	
	unsigned int i=0;
	unsigned int add_sectors = ((current_block*blocks_per_group*2));
	unsigned int in_buf=0;	
	
	//current=malloc(sizeof(struct ext2_inode));
	current=read_inode(n);
	if(current==NULL) {
		//print_inode(current);
		return;
	}
#if 0
	add_sectors+=start_sector;
//	printf("The add sector is %d\n",add_sectors);
	sector=INODE_TO_SECTOR(GET_INDEX(n));
	sector+=add_sectors;
//	printf("The sector is %d\n",sector);
	//offset=BLOCK_OFFSET(5);	
	//i=INODE_NUMBER(offset);
	//printf("THe inode is %d\n",i);
	read_device(device,(sector),4,buf);
	if(ROUNDS_OFF(GET_INDEX(n))){
		offset=0;
//		printf("It does\n");
	}
	else{
		offset=GET_OFFSET(n);
		printf("It doesnt offset is %d\n",offset);
	}
	memcpy(current,(buf+offset),sizeof(struct ext2_inode));
	
//	print_inode(current);
//	return NULL;
#endif
//	printf("Size in bytes %u pointer to first data block %u blocks count %d\n",current->i_size,current->i_block[0],current->i_blocks);
	//printf("%d\n",6/8);
	for(;current->i_block[i]!=0;i++){
#if 1 
		block=current->i_block[i];
		block=BLOCK_OFFSET(block);
		dsector=block/sector_size_bytes;
		dsector+=start_sector;
//	printf("The sector is %d\n",sector);
		dir=malloc(sizeof(struct ext2_dir_entry_2));
		if(dir==NULL) return NULL;
		read_device(device,dsector,2,buf);
		memcpy(dir,buf,sizeof(struct ext2_dir_entry_2));
//	printf("1 INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		block=dir->rec_len;		
//	if(n!=2)
	//	p=dir->inode;		
#endif
	
#if 1		
	while(dir->inode!=0) {
//	while(block < current->i_size) 
#if 1
	
		if( dir->file_type==2 ) {

			if(!strcmp(dir->name,".") || !strcmp(dir->name,"..")) {
				dots++;	
				if(!strcmp(dir->name,".")) {
				//	p=dir->inode;
					checkItself(dir,n);
				}
				else {
					
	
		//	printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
			dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d  parent is %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type,p);
					checkParent(dir,p);	
				}
			}		
			else {
			dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
//			printf("%s/",dir->name);
//	if(dots==2)
//		printf("\nDirectory has parent and self!\n");
			//if(n==2)
			//	p=2;
			//else
			//	p=dir->		
	
			checkInode(dir->inode,p);	
			printf("\n");
			}
#endif
			if(do_copy){
	//			dbg_p("-----DO COPY!!!!------------\n");
#if 0		
				memcpy((buf+in_buf),dir,sizeof(struct ext2_dir_entry_2));
				write_sectors(dsector,1,buf);
#endif		
				do_copy=0;
			}
	
	 }
		memcpy(dir,(buf+block),sizeof(struct ext2_dir_entry_2));
		in_buf+=block;
		block+=dir->rec_len;		
	}
	
#endif	
    }
 
#if 0
	while(dir->inode!=0 && dir->file_type==2) { 
	//	printf("2 INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		memcpy(dir,(buf+block),sizeof(struct ext2_dir_entry_2));
		
	if(!strcmp(dir->name,".") || !strcmp(dir->name,"..")) {
			
	}
	else {
			
			printf("%s/",dir->name);
			checkInode(dir->inode,p);


	}	
		block+=dir->rec_len;		
	}
#endif
		return current;
//#endif
}
void checkItself(struct ext2_dir_entry_2* dir,int itself)
{
	if(dir->inode==itself)
		 return;
	else{
		do_copy=1;
		printf("BAD inode number for '.'.\nChange inode number of '.' to %d",itself);
		dir->inode=itself;
	
	}
}

void checkParent(struct ext2_dir_entry_2* dir,int parent)
{

	if(dir->inode==parent)
;//		dbg_p("Points to parent\n");
	else {
		do_copy=1;
		printf("BAD PARENT POINTER for '..'\nChange inode number of '..' to %d\n",parent);		dir->inode=parent;

	}


}
void checkDirectories(void)
{

	checkInode(2,2); //2-->ROOT,2-->PARENT
	

}
void checkUnreferenced(void)
{



}
void checkLinkCount(void)
{


}
void checkBitMap(void)
{




}
void print_inode(struct ext2_inode* inode)
{	
	int i=0;
	char file_name[EXT2_NAME_LEN+1];
	for (i=0; i<EXT2_N_BLOCKS; i++)
		if (i < EXT2_NDIR_BLOCKS) /* direct blocks */
			printf("Block %2u : %u\n", i, inode->i_block[i]);
		else if (i == EXT2_IND_BLOCK) /* single indirect block */	
			printf("Single : %u\n", inode->i_block[i]);	
		else if (i == EXT2_DIND_BLOCK) /* double indirect block */
			printf("Double : %u\n", inode->i_block[i]);
		else if (i == EXT2_TIND_BLOCK) /* triple indirect block */
			printf("Triple : %u\n", inode->i_block[i]);	
	

}
struct ext2_inode* read_inode( int inode_no)
{

int group_no = inode_no/super->s_inodes_per_group;
int inode_offs = inode_no - (group_no * inodes_per_group) - 1;
struct ext2_inode* current=malloc(sizeof(struct ext2_inode));
unsigned int sector=0;
unsigned int current_block = GET_BLOCK_GROUP(inode_no); 	
unsigned int i=0;
unsigned int add_sectors = ((current_block*blocks_per_group*2));
unsigned int in_buf=0;	
unsigned int offset=0;	
	add_sectors+=start_sector;
	sector=INODE_TO_SECTOR(GET_INDEX(inode_no));
	sector+=add_sectors;
	if(ROUNDS_OFF(GET_INDEX(inode_no))){
		offset=0;
//		printf("It does\n");
	}
	else{
		offset=GET_OFFSET(inode_no);
		printf("It doesnt offset is %d\n",offset);
	}

	lseek64(device,(sector*sector_size_bytes+offset), SEEK_SET);

	read(device, current, sizeof(struct ext2_inode));
	return current;
}
