/* 

Author: Khushal Shah
Date: 2/18/2014

A file system check utility.

Part 1: Read partition table and get information about various partition 

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

#define DEBUGx
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
static int first_block; //first block Superblock(SB) from 
#define	START_SECTOR
static int start_sector; //Start sector of PARTITION
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
static unsigned int* my_inode_map; 
static unsigned char* my_block_bitmap;
// n -> inode number
//offset -> disk offset to get inode number
#define BLOCK_OFFSET(block)  (BASE_OFFSET + (block-1)*block_size)
#define DISK_OFFSET(n) (first_block + (n-1)*inode_size)
#define INODE_NUMBER(offset) ((offset-5120)/128) + 1
#define INODE_TO_SECTOR(n) (DISK_OFFSET(n))/512
#define ROUNDS_OFF(n) ( (DISK_OFFSET(n)%512==0)?(DISK_OFFSET(n)/512):0)
#define GET_OFFSET(n) (DISK_OFFSET(n)%512)

#define GET_BLOCK_GROUP(n) (n-1)/inodes_per_group
#define GET_INDEX(n) ( ((n)%inodes_per_group==0)?(n-1)%inodes_per_group:n%inodes_per_group)
#define GET_CONT_BLOCK(index) ( (index*inode_size) / block_size)
#define ROOT 2
#define CHECK_BIT(x,pos) (x & (1<<pos))
 
static int device;
static const sector_size_bytes = 512;
static struct ext2_super_block* super=NULL;
static struct ext2_group_desc* group_desc=NULL;
static struct partition_table* head=NULL;
static unsigned blocks_per_group;
static unsigned total_block_count;
static unsigned lost_and_found;
static unsigned group_count;
static int count=0;
static int level=0;
static int* my_block_map;

/*Struct to keep track of the partition table and each partition in MBR and EBR*/
struct partition_table{
		
		int p_no;	//partition number
		struct partition p;
		struct partition_table* next;

};

/* Argument processing */
void fillArgs(int argc,char** argv,int* p,int *f,char** buf);


/*Partition functions Part I*/
void partitionTableEntry(int partition,int sector);
struct partition_table* get_partition(struct partition_table*, int );
void addExtendedPartition(struct partition_table** head,struct partition_table* given);
void assign_partition(struct partition_table** head);
void buildList(struct partition_table** head,int sector);
void addToList(struct partition_table** head,struct partition_table* current);
void startCheck(struct partition_table* temp);
void checkForExtended(struct partition_table** head);


/*Set/Unset Calls*/	
void writeBitMapForBlock(int block); //Write the bitmap to the disc
void setBitForMyBlock(unsigned char* bitmap,int block);
void setBitForInode(unsigned char* bitmap,int inode);
void setBitForBlock(unsigned char* bitmap,int block);
void unsetBitForInode(unsigned char* bitmap,int inode);
void unsetBitForBlock(unsigned char* bitmap,int block);
void clearHashMap(unsigned int* hash_map);
/*Print Functions*/

void print_superBlock(struct ext2_super_block* s);
void print_groupDescriptor(struct ext2_group_desc* gd);
void print_inode(struct ext2_inode* inode);
void print_directories(int inode);
void printList(struct partition_table** head);

/*Read write calls*/
void write_block(int block,unsigned char* buf);
void* read_block(int block);
void readSuperBlock(struct partition_table* temp);
void readGroupDescriptor(char* buf);
void write_inode(int inode_no,struct ext2_inode* i);
struct ext2_inode* read_inode( int inode_no);

/*Getter Functions*/
void getBitMapForInode(int inode);
int getFileType(int inode);
int getEntrySize(char * name);
int getBlockForDirectory(int inode);
void getBitMapForBlock(int block_no);

/* FILE SYSTEM CHECK FUNCTIONS */

void checkFS(int f);
void checkDirectories(void);
void checkUnreferenced(int n);
int checkItself(struct ext2_dir_entry_2** dir,int itself);
void checkLinkCount(int inode);
void checkBlockBitMap(int inode);
int  checkParent(struct ext2_dir_entry_2** dir,int parent);
int checkInodeAllocated(unsigned char* bitmap,int inode);
void checkFileBlocks(int inode);
int checkBlockAllocated(unsigned char* bitmap,int block);
void checkUnreferencedCount(void);
int checkValidInode(int inode);
struct ext2_inode* checkInode(unsigned int n,unsigned int parent);
void addToLostAndFound(int inode);
void crossBlockCheck(void);
void triplyIndirect(int block);
void singlyIndirect(int block);
void doublyIndirect(int block);
int checkBitForMyBlock(unsigned char* bitmap,int block);



void print_usage(void)
{
	printf("Please enter format as follows:\n");
	printf("./myfsck -p <partition number> OR");
	printf("./myfsck -f <partition number>");
	printf(" AND -i <disk image>\n");
	exit(0);

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
	struct ext2_inode* inode=NULL;
	char buf[sector_size_bytes*6];
	
	dbg_p("%s: BUFFER size is %lu\n",__func__,sizeof(buf));	
	/*Malloc-read-copy*/
	super=malloc(sizeof(struct ext2_super_block));
	read_device(device,temp->p.start_sect,6,buf);
	memcpy(super,(buf+1024),sizeof(struct ext2_super_block));

	if(super->s_magic == EXT2_SUPER_MAGIC){
			
		dbg_p("Magic number is %04x\n",super->s_magic);

	}
	else{
			
		printf("SuperBlock Magic number does not match!!\n");
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
	group_count=1+(total_block_count-1)/blocks_per_group;
	readGroupDescriptor((buf));

}


void readGroupDescriptor(char* buf)
{
	int64_t lret;
	int64_t sector_offset;
	ssize_t ret;

	/*malloc & copy*/
	group_desc=malloc(sizeof(struct ext2_group_desc)*group_count);
	memcpy(group_desc,(buf+2048),sizeof(struct ext2_group_desc)*group_count);
	first_block=BLOCK_OFFSET(group_desc->bg_inode_table);
	block_bitmap=malloc(block_size);
	inode_bitmap=malloc(block_size);
	my_inode_map=calloc((total_inodes+1),sizeof(int));
	lret=1+(total_block_count-1)/8;
	printf("lret %lu\n",lret);
	my_block_bitmap=calloc(lret,sizeof(char));
	dbg_p("First block %d\n",first_block);
	
}

void getBitMapForInode(int inode)
{
	int64_t lret;
	int64_t sector_offset;
	ssize_t ret;
	int group_no= (inode-1)/inodes_per_group;
	int inode_off = (inode-1) %inodes_per_group;

	
	sector_offset=BLOCK_OFFSET(group_desc[group_no].bg_inode_bitmap)+start_sector*sector_size_bytes;
	
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

}

void getBitMapForBlock(int block)
{
	int64_t lret;
	int64_t sector_offset;
	ssize_t ret;
	int group_no= (block-1)/blocks_per_group;
	int block_off = (block-1) %blocks_per_group;
	
	sector_offset=BLOCK_OFFSET(group_desc[group_no].bg_block_bitmap)+start_sector*sector_size_bytes;
	
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
	
}

void writeBitMapForBlock(int block)
{
	int64_t lret;
	int64_t sector_offset;
	ssize_t ret;
	int group_no= (block-1)/blocks_per_group;
	int block_off = (block-1) %blocks_per_group;
		
	sector_offset=BLOCK_OFFSET(group_desc[group_no].bg_block_bitmap)+start_sector*sector_size_bytes;
	
	if( (lret=lseek64(device,sector_offset,SEEK_SET))!=sector_offset) {

		fprintf(stderr,"%s Seek to position %"PRId64" failed: "
				"returned %"PRId64"\n",__func__,sector_offset,lret);
		exit(-1);
	}
	if( (ret=write(device,block_bitmap,block_size))!=block_size) {
		
		fprintf(stderr,"%s Read sector %"PRId64"  failed: "
				"returned %"PRId64"\n",__func__,sector_offset,ret);
		exit(-1);

	}
	
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

int checkItself(struct ext2_dir_entry_2** dir,int itself)
{
	if((*dir)->inode==itself)
		 return 0;
	else{
		
		printf("BAD inode number for '.'.\nChange inode number of '.' to %d",itself);
		(*dir)->inode=itself;
		return 1;	
	}

}

int checkParent(struct ext2_dir_entry_2** dir,int parent)
{

	if((*dir)->inode==parent)
		return 0;//		dbg_p("Points to parent\n");
	
	else {
		
		printf("BAD PARENT POINTER for '..'\nChange inode number of '..' to %d\n",parent);		
		(*dir)->inode=parent;
		return 1;
	}


}

void checkDirectories(void)
{
	checkInode(2,2); //2-->ROOT,2-->PARENT
	

}

void checkUnreferencedCount(void)
{
	int i=0;
	struct ext2_inode* current=NULL;
	
	for(i=2;i<super->s_inodes_count;i++)
	{
		current=read_inode(i);
				
				if(current->i_links_count!=0 && my_inode_map[i]==0)
				{
					addToLostAndFound(i);
					printf("Unreferenced inode %d adding it to /lost+found\n",i);
					my_inode_map[i]++;	
				
					if(S_ISDIR(current->i_mode)) {
				
						checkInode(i,lost_and_found);
						checkUnreferenced(i);;	
				
					}
	
				}	
		free(current);
	}

}


int getBlockForDirectory(int inode)
{

	struct ext2_inode* current=NULL;
	char* buf=NULL;
	int i=0;
	int block=0;
	int dsector=0;
	int in_buf=0;
	struct ext2_dir_entry_2* prev;
	struct ext2_dir_entry_2* dir=NULL;
	char* tracker=NULL;	
	current=read_inode(lost_and_found);
	char file_name[EXT2_NAME_LEN+1]={0};
	sprintf(file_name,"%d",inode);
	int est_size=getEntrySize(file_name);
	int est_len=0;

	for(;current->i_block[i]!=0;i++) {

		in_buf=0; 
		block=current->i_block[i];
		buf=read_block(block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
		

		
		while(size < block_size && dir->inode!=0) {

				printf("%s: INODE Number is %d Name is: %s Rec %d dir_type %d \n",__func__,dir->inode,dir->name,dir->rec_len,dir->file_type);
			
					
			est_len=getEntrySize(dir->name);
			if(dir->rec_len-est_len >=est_size) {
						
					free(buf);
					free(current);
					return block;
			}
			dir=(void*)dir+dir->rec_len;
			size+=dir->rec_len;
		}

		free(buf);
	}
		free(current);
}

int getEntrySize(char * name)
{
	int len=0;
	int i=0;
	len=8+strlen(name); 
	
	if((i=len%4)!=0)
		len=len-i+4;


	return len;
}

int getFileType(int inode)
{
	struct ext2_inode* current=NULL;
	//int file_type=0;
	int m;
	current=read_inode(inode);
	m=current->i_mode;
	int file_type=0;
	
	if(S_ISREG(m))
		file_type=1; 
	else if( S_ISDIR(m))
		 file_type=2; 
	else if ( S_ISCHR(m))
		file_type=3; 
	else if( S_ISBLK(m))
		file_type=4; 
	else if( S_ISFIFO(m))
		file_type=5; 
	else if(S_ISSOCK(m))
		file_type=6; 
	else if( S_ISLNK(m))
		file_type=7; 
	
	free(current);
	return file_type;
}


void print_directories(int inode)
{
	struct ext2_inode* current;
	current=read_inode(inode);
	struct ext2_dir_entry_2* dir;
	unsigned char* buf;
	int i=0;	
	int block=0;
	printf("***************** %s *************\n",__func__);
	for(;current->i_block[i]!=0;i++) {

		block=current->i_block[i];
		buf=read_block(block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
		

		
		while(size < block_size && dir->inode!=0) {

//				printf("INODE Number is %d Name is: %s Rec %d dir_type %d \n",dir->inode,dir->name,dir->rec_len,dir->file_type);
			
					
				if( dir->file_type==2 ) {
	
					if(!(strcmp(dir->name,".."))|| !(strcmp(dir->name,".")));
					else {
							printf("%s/",dir->name);
							print_directories(dir->inode);
						}
					}	
			
			dir=(void*)dir+dir->rec_len;
			size+=dir->rec_len;
		}
		printf("\n");
		free(buf);
	}
		free(current);
	

	printf("***************** %s *************\n",__func__);


}

void setBitForInode(unsigned char* bitmap,int inode)
{
	int group_no= (inode-1)/inodes_per_group;
	int inode_off = (inode-1) %inodes_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (inode_off)/8;
	shift_index=(inode_off)%8;
	
	bitmap[bit_map_index] |= 1<<shift_index;


}

void setBitForBlock(unsigned char* bitmap,int block)
{
	int group_no= (block-1)/blocks_per_group;
	int block_off = (block-1) %blocks_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (block_off)/8;
	shift_index=(block_off)%8;
	
	bitmap[bit_map_index] |= 1<<shift_index;


}

void setBitForMyBlock(unsigned char* bitmap,int block)
{
	int bit_map_index=(block-1)/8;
	int shift_index=(block-1)%8;
	bitmap[bit_map_index] |= 1<<shift_index;

}

int checkBitForMyBlock(unsigned char* bitmap,int block)
{
	int bit_map_index=(block-1)/8;
	int shift_index=(block-1)%8;
	
	if(bitmap[bit_map_index]& (1<<shift_index))
		return 1;
	else
		return 0;


}

void unsetBitForInode(unsigned char* bitmap,int inode)
{
	int group_no= (inode-1)/inodes_per_group;
	int inode_off = (inode-1) %inodes_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (inode_off-1)/8;
	shift_index=(inode_off-1)%8;
	
	bitmap[bit_map_index] &= ~(1<<shift_index);


}

void unsetBitForBlock(unsigned char* bitmap,int block)
{

	int group_no= (block-1)/blocks_per_group;
	int block_off = (block-1) %blocks_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (block_off)/8;
	shift_index=(block_off)%8;
	
	bitmap[bit_map_index] &= ~(1<<shift_index);

}

int checkInodeAllocated(unsigned char* bitmap,int inode)
{

	int group_no= (inode-1)/inodes_per_group;
	int inode_off = (inode-1) %inodes_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (inode_off)/8 ;
	shift_index=(inode_off)%8;
	
//	printf("Bit_map_index %d\nShift Index %d\nInode_off %d Bitmap %d\n",bit_map_index,shift_index,inode_off,bitmap[bit_map_index]);
	
	if( (((bitmap[bit_map_index])&(1<<shift_index))))
				return 1;//	printf("Inode %d aLlocated!\n",inode);
	else{
			
		printf("%d Not allocated \n",inode);
		
		return 0;
	}



}

void crossBlockCheck(void)
{

	int group_no;//= (block)/blocks_per_group;
	int block_off;// = (block) %blocks_per_group;
	int bit_map_index;	
	int shift_index;
	int i=1;
	int x=0;
	int y=0;
	//bit_map_index = (block_off)/8 ;
	//shift_index=(block_off)%8;
		
	for(;i<=total_block_count;i++) {
		
	group_no= (i-1)/blocks_per_group;
	block_off = (i-1) %blocks_per_group;
	bit_map_index = (block_off)/8 ;
	shift_index=(block_off)%8;
		getBitMapForBlock(i);		
//	printf("Bit_map_index %d\nShift Index %d\nInode_off %d Bitmap %d\n",bit_map_index,shift_index,inode_off,bitmap[bit_map_index]);
	
		if( (((block_bitmap[bit_map_index])&(1<<shift_index)))) {
			
			//printf("block %d aLlocated!\n",block);
			//	return 1;
			x++;	
		}
		else{
			y++;
//			printf("%d Not allocated \n",block);
		
			
		}
 		if(checkBitForMyBlock(my_block_bitmap,i) && !checkBlockAllocated(block_bitmap,i)){
				printf("%s: Block %d\n",__func__,i);
				setBitForBlock(block_bitmap,i);
				//write_block(group_count+1,block_bitmap);
				writeBitMapForBlock(i);
		}//else 	printf(" Not allocate %d ",i);
			
		
	}
	
	printf("Total block count %d Allocated %d not allocated %d\n",total_block_count,x,y);
	print_superBlock(super);
}

int checkBlockAllocated(unsigned char* bitmap, int block)
{

	int group_no= (block-1)/blocks_per_group;
	int block_off = (block-1) %blocks_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (block_off)/8 ;
	shift_index=(block_off)%8;
		
//	dbg_p("Bit_map_index %d\nShift Index %d\nInode_off %d Bitmap %d\n",bit_map_index,shift_index,inode_off,bitmap[bit_map_index]);
	
	if( (((bitmap[bit_map_index])&(1<<shift_index)))) {
			
			//printf("block %d aLlocated!\n",block);
			return 1;
	}
	else{
			
		printf("%s: %d Not allocated \n",__func__,block);
		
		return 0;
	}

}

int checkValidInode(int inode)
{
	if(ZERO <inode && inode<=total_inodes)
		return 1;
	else{
		printf("%s: Not valid %d\n",__func__,inode);
		return 0;
	}


}
int checkValidBlock(unsigned int block)
{
	if(ZERO <block && block <= total_block_count)
		return 1;
	else{
		printf("%s: Not valid %d\n",__func__,block);
		return 0;
	}


}

void checkBlockBitMap(int inode)
{
	struct ext2_inode* current;
	struct ext2_dir_entry_2* dir;
	int i=ZERO;
	char* buf;
	int block=ZERO;	
	
	if(!checkValidInode(inode))
			return;

	current=read_inode(inode);
		
	for(;current->i_block[i]!=0;i++)
	{
		block=current->i_block[i];
		if(!checkValidBlock(block)) {
			
			free(current);
			return;

		}
		count++;
		buf=read_block(block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
		setBitForMyBlock(my_block_bitmap,block);
	//	printf("Block[%d]: %d\n",i,block);	
		while(size < block_size && dir->inode!=0)
		{

				//printf("INODE Number is %d Name is: %s Rec %d dir_type %d \n",dir->inode,dir->name,dir->rec_len,dir->file_type);
				checkFileBlocks(dir->inode);
				if( dir->file_type==2 ) {
				
					if(!(strcmp(dir->name,".."))|| !(strcmp(dir->name,"."))) {
							
					
					}
					else {
							//printf("%s/",dir->name);
							checkBlockBitMap(dir->inode);
						}
				}

			dir=(void*)dir+dir->rec_len;
			size+=dir->rec_len;

		}
		
		free(buf);
	}
 
		free(current);

	
}
void checkFileBlocks(int inode)
{
	struct ext2_inode* current;
	int size=0;
	if(!checkValidInode(inode))
			return;
	current=read_inode(inode);
	int i=ZERO;
	int block=ZERO;

	for(;current->i_block[i]!=ZERO && i!=EXT2_NDIR_BLOCKS;i++)
	{

		block=current->i_block[i];
		if(!checkValidBlock(block)) {
			
		
			free(current);
			return;

		}
			count++;
			setBitForMyBlock(my_block_bitmap,block);

	}

	if(i < EXT2_NDIR_BLOCKS) {  free(current); return;}	

	if(current->i_block[i]!=ZERO) {
		
		block=current->i_block[i];
		if(!checkValidBlock(block)) {
			
			free(current);
			return;

		}

			count++;
			setBitForMyBlock(my_block_bitmap,block);

		singlyIndirect(block);	
		i++;
	}
	else {
		free(current);
	 	return;
	}
//	printf("%s: DOUBLY and is %d\n",__func__,current->i_block[i]);	
	if(current->i_block[i]!=ZERO) {
		block=current->i_block[i];
		if(!checkValidBlock(block)) {
			
			free(current);
			return;

		}
			count++;
			setBitForMyBlock(my_block_bitmap,block);
		
	
		doublyIndirect(block);
		i++;
	}
	else {
		free(current);
		return;
	}
	//printf("%s: TRIPLY\n",__func__);	
	if(current->i_block[i]!=ZERO) {
		

		block=current->i_block[i];
		if(!checkValidBlock(block)) {
			
			free(current);
			return;

		}
			count++;
			setBitForMyBlock(my_block_bitmap,block);
		
		triplyIndirect(current->i_block[i]);

	}
	free(current);	
	return;
}

void singlyIndirect(int block_no)
{		
		unsigned int* indirect=(unsigned int *)read_block(block_no);
		unsigned int* temp= indirect;
		int block=0;
		while(temp!=NULL && *temp!=ZERO){
		
			block=*temp;
			if(!checkValidBlock(block)) {
				
				free(indirect);
				return;

			}
			
				
			setBitForMyBlock(my_block_bitmap,block);
			temp++;
		}
		
		free(indirect);
		
}

void doublyIndirect(int block_no)
{
		unsigned int* indirect=(unsigned int *)read_block(block_no);
		unsigned int* temp= indirect;
		int block=0;
		while(temp!=NULL && *temp!=ZERO){
		
			block=*temp;
			
			if(!checkValidBlock(block)) {
			
				free(indirect);
				return;

			}
			
			setBitForMyBlock(my_block_bitmap,block);
			count++;
			singlyIndirect(block);
			temp++;
		}

		free(indirect);
}

void triplyIndirect(int block_no)
{
		unsigned int *indirect=(unsigned int *)read_block(block_no);
		unsigned int* temp= indirect;
		int block=0;
	
		while(temp!=NULL && *temp!=ZERO){

			block=*temp;
			if(!checkValidBlock(block)) {
			
				free(indirect);
				return;
			}

			setBitForMyBlock(my_block_bitmap,block);
			doublyIndirect(block);
			temp++;
			
		}

		free(indirect);
}

void clearHashMap(unsigned int* hash_map)
{
	int i=0;
	for(;i<=total_inodes;i++)
		hash_map[i]=ZERO;
	
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

void* read_block(int block)
{
	void* buf=calloc(1024,sizeof(char));
	
	lseek(device,BLOCK_OFFSET(block)+start_sector*sector_size_bytes,SEEK_SET);
	read(device,buf,block_size);

	return buf;

}

void write_block(int block,unsigned char* buf)
{
	
	lseek(device,BLOCK_OFFSET(block)+start_sector*sector_size_bytes,SEEK_SET);
	write(device,buf,block_size);

}

struct ext2_inode* read_inode( int inode_no)
{

	int group_no= (inode_no-1)/inodes_per_group;
	int inode_off = (inode_no-1) %inodes_per_group;
	struct ext2_inode* current=malloc(sizeof(struct ext2_inode));
	unsigned int go_to=0;
	
	go_to=BLOCK_OFFSET(group_desc[group_no].bg_inode_table)+(start_sector*sector_size_bytes)+inode_off*inode_size;

	lseek64(device,go_to, SEEK_SET);
	read(device, current, sizeof(struct ext2_inode));

	return current;
}

void write_inode(int inode_no,struct ext2_inode* i)
{

	int group_no= (inode_no-1)/inodes_per_group;
	int inode_off = (inode_no-1) %inodes_per_group;
	unsigned int go_to=0;
	
	go_to=BLOCK_OFFSET(group_desc[group_no].bg_inode_table)+(start_sector*sector_size_bytes)+inode_off*inode_size;
	lseek64(device,go_to, SEEK_SET);

	write(device, i, sizeof(struct ext2_inode));
	
}
