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
static unsigned int* my_hash_map;
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
static int level=0;
static struct ext2_super_block* super=NULL;
static struct ext2_group_desc* group_desc=NULL;
static struct partition_table* head=NULL;
static unsigned int fs_size;
static unsigned blocks_per_group;
static unsigned total_block_count;
static unsigned int do_copy=0;
static unsigned lost_and_found;
static unsigned group_count;
static int level_dir=1;
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
void checkUnreferenced(int n);
void clearHashMap(unsigned int* hash_map);
int checkItself(struct ext2_dir_entry_2** dir,int itself);
void checkLinkCount(int inode);
void checkBitMap(int inode);
int  checkParent(struct ext2_dir_entry_2** dir,int parent);
struct ext2_inode* read_inode( int inode_no);
void write_inode(int inode_no,struct ext2_inode* i);
void unsetBit(unsigned char* bitmap,int inode);
void setBit(unsigned char* bitmap,int inode);
int checkAllocated(unsigned char* bitmap,int inode);
void checkMap(unsigned char* bitmap);
void addToLostAndFound(int inode);
void addCP(int inode,int c,int p);
void checkHash(unsigned int* hash_map);
void getBitMapForInode(int inode);
void getBitMapForBlock(int block_no);
int checkBlockAllocated(int block);
void write_block(int block,unsigned char* buf);
void* read_block(int block);
void checkUnreferencedCount(void);
int getFileType(int inode);
int getEntrySize(char * name);
int getBlock(int inode);
void print_directories(int inode);

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
			free(temp1);		
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
	static int i=1;
	
	buildList(&head,ZERO);		
	assign_partition(&head);	
	
	dbg_p("f-->%d\n",f);
	
	if(f<0)
		print_usage();	
	
	if(f==ZERO){
		printf("Check for every ext2 partition\n");	
		for(;((temp=get_partition(head,i))!=NULL);i++)
		{
			if(temp->p.sys_ind == LINUX_EXT2_PARTITION)
					startCheck(temp);
			else{
				dbg_p("Not an EXT2 Partition\n");
			
			}	
		}	
	
		return;		
		
	}
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
					//	lost_and_found=11;
						printf("----------------- 1 PASSS--------------------------------\n");
						
					case 2:
						checkUnreferenced(ROOT);
						checkUnreferencedCount();
						printf("-----------------  PASSS 2--------------------------------\n");
						//checkUnreferenced(ROOT);
						
					case 3:
						checkLinkCount(ROOT);
						
					case 4:
						//checkBitMap(ROOT);
						
					default:
							pass=4;
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
	group_count=1+(total_block_count-1)/blocks_per_group;
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
	group_desc=malloc(sizeof(struct ext2_group_desc)*group_count);
	memcpy(group_desc,(buf+2048),sizeof(struct ext2_group_desc)*group_count);
	first_block=BLOCK_OFFSET(group_desc->bg_inode_table);
	block_bitmap=malloc(block_size);
	inode_bitmap=malloc(block_size);
	my_hash_map=calloc((total_inodes+1),sizeof(int));
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
	unsigned char* buf;
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int offset;
	unsigned int block=0;
	unsigned int i=0;
	unsigned int in_buf=0;	
	
	char file_name[EXT2_NAME_LEN+1];
	//current=malloc(sizeof(struct ext2_inode));
	current=read_inode(n);
	if(current==NULL) {
		//print_inode(current);
		return;
	}
	//printf("Size in bytes %u pointer to first data block %u blocks count %d links count %d\n",current->i_size,current->i_block[0],current->i_blocks,current->i_links_count);
	for(;current->i_block[i]!=0;i++){
 
		in_buf=0;	
		block=current->i_block[i];
		buf=(unsigned char*)read_block(block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
		size=0;	
#if 1		
//	while(dir->inode!=0) 
	while(size < block_size && dir->inode!=0) { 
#if 1
	
		if( dir->file_type==2 ) {

			if(!strcmp(dir->name,".") || !strcmp(dir->name,"..")) {
				
				if(!strcmp(dir->name,".")) {
					//	p=dir->inode;
					//printf("%s\n",dir->name);
					if(checkItself(&dir,n))
						write_block(block,buf);
					
				}
				else {
				
					memcpy(file_name,dir->name,dir->name_len);
					file_name[dir->name_len]='\0';
//					printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
					dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d  parent is %d\n",dir->inode,file_name,dir->rec_len,dir->file_type,p);
					if(checkParent(&dir,p))
						write_block(block,buf);	
						
				//	p=dir->inode;
				}
			}		
			else {
			
				memcpy(file_name,dir->name,dir->name_len);
				file_name[dir->name_len]='\0';
					
				if(!strcmp(file_name,"lost+found")){
					
					lost_and_found=dir->inode;
					//printf("LostFoundThe inode number is %d and %s\n",dir->inode,file_name);
				}
				printf("%s:INODE Number is %d Name is: %s Rec %d dir_type %d\n",__func__,dir->inode,file_name,dir->rec_len,dir->file_type);
				
			dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,file_name,dir->rec_len,dir->file_type);
			//printf("%s/",dir->name);
//	if(dots==2)
//		printf("\nDirectory has parent and self!\n");
			//	if(level_dir<2)
			//		p=2;
			//	else
			//		p=n;		
			//	level_dir++;
				p=n;		
				checkInode(dir->inode,p);	
			//printf("\n");
			}
#endif
	
	 	}
		
		dir=(void*)dir+dir->rec_len;
		size+=dir->rec_len;
	}
			/*if(do_copy){
				printf("-----DO COPY!!!!------------\n");
 		
			//	memcpy((buf+in_buf),dir,sizeof(struct ext2_dir_entry_2));
			//	write_sectors(dsector,2,(buf));
				write_block(block,buf);
				do_copy=0;
				print_directories(11);
			}
		*/
#endif
		free(buf);		
    }
		free(current); 
		return current;
//#endif
}
int checkItself(struct ext2_dir_entry_2** dir,int itself)
{
	if((*dir)->inode==itself)
		 return 0;
	else{
		do_copy=1;
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
		do_copy=1;
		printf("BAD PARENT POINTER for '..'\nChange inode number of '..' to %d\n",parent);		
		(*dir)->inode=parent;
		return 1;
	}


}

void checkDirectories(void)
{
	level_dir=1;
	checkInode(2,2); //2-->ROOT,2-->PARENT
	

}
void checkUnreferenced(int n)
{
	
	struct ext2_inode* current=NULL;
	char* buf;
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int dsector=0;
	unsigned int block=0;
	unsigned int i=0;
	unsigned int in_buf=0;	
	static int level=0;
	static unsigned int link_count=0;
	static unsigned char* bitmap;
	current=read_inode(n);
	
	if(current==NULL) {
		return;
	}
	if(!level){
	//	printf("Gotcha!");
		level=1;
	}
	link_count++;
	//printf("Size in bytes %u pointer to first data block %u blocks count %d links count %d\n",current->i_size,current->i_block[0],current->i_blocks,current->i_links_count);
	for(;current->i_block[i]!=0;i++){

		in_buf=0;	
		block=current->i_block[i];
		buf=read_block(block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
#if 1		
		while(size < block_size && dir->inode!=0) {
#if 1		
			my_hash_map[dir->inode]++;	
			
			if( dir->file_type==2 ) {
				
			
				if(!strcmp(dir->name,".") || !strcmp(dir->name,"..")) {
					
					
	
					dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d  parent is %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type,p);
						
				}		
				else {
					checkUnreferenced(dir->inode);	
			
				}
#endif
	
	 		}
		
		//memcpy(dir,(buf+block),sizeof(struct ext2_dir_entry_2));
		dir=(void*)dir+dir->rec_len;
		size+=dir->rec_len;
		}
		dbg_p("Here\n");	
		free(buf);	
#endif	
    }
	
	free(current);	
	link_count--;
	//if(link_count==0)	
	//	checkUnreferencedCount();


}
void checkUnreferencedCount(void)
{
	int i=0;
	struct ext2_inode* current=NULL;
	for(i=2;i<super->s_inodes_count;i++)
	{
		current=read_inode(i);
		//if((S_ISDIR(current->i_mode))) {
				
				if(current->i_links_count!=0 && my_hash_map[i]==0)
				{
					addToLostAndFound(i);
				printf("Unreferenced inode %d adding it to /lost+found\n",i);
				my_hash_map[i]++;	
				
				if(S_ISDIR(current->i_mode)) {
				
					checkInode(i,lost_and_found);
					//checkDirectories();
					//clearHashMap(my_hash_map);
					checkUnreferenced(i);;	
				
				}
	
				}	
		//}
			free(current);
	}

}
void checkMap(unsigned char* bitmap)
{
	int i=0;
	struct ext2_inode* current;//; malloc(sizeof(struct ext2_inode));
//	printf("checkMap, %d\n",super->s_inodes_count);
	for(i=2;i < super->s_inodes_count;i++)
	{
		
		current=read_inode(i);
		if((S_ISDIR(current->i_mode))) {
		//printf("D %d\n",i);
				getBitMapForInode(i);	
			if( (checkAllocated(bitmap,i)==1) && (checkAllocated(inode_bitmap,i)==1) )
					;
			else{
			//	if(checkAllocated(inode_bitmap,i)==0)
			//		continue;
				
				printf("Unreferenced inode %d adding it to /lost+found\n",i);
				addToLostAndFound(i);
				checkDirectories();
				checkUnreferenced(i);
					
				}
		}
		free(current);
	}	

}

int getBlock(int inode)
{

	struct ext2_inode* current=NULL;
	char* buf=NULL;
	//char* buf[];
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
	//buf=malloc(current->i_size);
	//prev=malloc(sizeof(struct ext2_dir_entry_2));
	for(;current->i_block[i]!=0;i++) {

		in_buf=0; 
		block=current->i_block[i];
		buf=read_block(block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
		

		
		while(size < block_size && dir->inode!=0) {

				printf("%s: INODE Number is %d Name is: %s Rec %d dir_type %d \n",__func__,dir->inode,dir->name,dir->rec_len,dir->file_type);
			
					
			//	if( dir->file_type==2 ) {
				
			//		memcpy(prev,dir,sizeof(struct ext2_dir_entry_2));
	
			//	}	
			est_len=getEntrySize(dir->name);
			if(dir->rec_len-est_len >=est_size) {
						
					free(buf);
					free(current);
					return block;
			}
			//in_buf=dir->rec_len;
			dir=(void*)dir+dir->rec_len;
			size+=dir->rec_len;
		}
	//printf("Inbuf:%d rec_len:%d\n",in_buf,prev->rec_len); 		
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

void addToLostAndFound(int inode)
{

	unsigned char* buf;
	//char* buf[];
	int i=0;
	int block=0;
	struct ext2_dir_entry_2* dir;
	int get_block=getBlock(inode);	
	char file_name[EXT2_NAME_LEN+1]={0};
	sprintf(file_name,"%d",inode);
	int est_size=getEntrySize(file_name);
	int est_len=0;
	int new_rec_len=0;

		buf=read_block(get_block);
		dir=(struct ext2_dir_entry_2*)buf;
		unsigned int size=0;
		

		
	while(size < block_size && dir->inode!=0) {

						//printf("INODE Number is %d Name is: %s Rec %d dir_type %d \n",dir->inode,dir->name,dir->rec_len,dir->file_type);
			
	//		if( dir->file_type==2 ) {
				
	//		memcpy(prev,dir,sizeof(struct ext2_dir_entry_2));
	
	//		}	
			est_len=getEntrySize(dir->name);
			
			if(dir->rec_len-est_len >=est_size) {
					
					new_rec_len=dir->rec_len-est_len;
					break;	
								
			}
	
			dir=(void*)dir+dir->rec_len;
			size+=dir->rec_len;
	}
	
	dir->rec_len=est_len;
	dir=(void*)dir+dir->rec_len;
	dir->inode=inode;
	sprintf(dir->name,"%d",inode);
	dir->file_type=getFileType(inode);	
	dir->rec_len=new_rec_len;
	dir->name_len=strlen(dir->name);
	write_block(get_block,buf);	
//	print_directories(11);
	free(buf);
		
		

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

				printf("INODE Number is %d Name is: %s Rec %d dir_type %d \n",dir->inode,dir->name,dir->rec_len,dir->file_type);
			
					
				if( dir->file_type==2 ) {
				
			//		memcpy(prev,dir,sizeof(struct ext2_dir_entry_2));
					if(!(strcmp(dir->name,".."))|| !(strcmp(dir->name,".")));
					else {
							printf("%s/",dir->name);
							print_directories(dir->inode);
						}
					}	
			
			//in_buf=dir->rec_len;
			dir=(void*)dir+dir->rec_len;
			size+=dir->rec_len;
		}
		printf("\n");
	//printf("Inbuf:%d rec_len:%d\n",in_buf,prev->rec_len); 		
		free(buf);
	}
		free(current);
	

	printf("***************** %s *************\n",__func__);


}
void setBit(unsigned char* bitmap,int inode)
{
	//int group_no= inode/inodes_per_group;
	//int inode_off = inode - (group_no*inodes_per_group) -1;
	int group_no= (inode-1)/inodes_per_group;
	int inode_off = (inode-1) %inodes_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (inode_off)/8;
	shift_index=(inode_off)%8;
	
	bitmap[bit_map_index] |= 1<<shift_index;


}
void unsetBit(unsigned char* bitmap,int inode)
{
	//int group_no= inode/inodes_per_group;
	//int inode_off = inode - (group_no*inodes_per_group) -1;
	int group_no= (inode-1)/inodes_per_group;
	int inode_off = (inode-1) %inodes_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (inode_off-1)/8;
	shift_index=(inode_off-1)%8;
	
	bitmap[bit_map_index] &= ~(1<<shift_index);


}
int checkAllocated(unsigned char* bitmap,int inode)
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
int checkBlockAllocated(int block)
{

	int group_no= (block-1)/blocks_per_group;
	int block_off = (block-1) %blocks_per_group;
	int bit_map_index;	
	int shift_index;
	bit_map_index = (block_off)/8 ;
	shift_index=(block_off)%8;
		
	getBitMapForBlock(block);		
//	printf("Bit_map_index %d\nShift Index %d\nInode_off %d Bitmap %d\n",bit_map_index,shift_index,inode_off,bitmap[bit_map_index]);
	
	if( (((block_bitmap[bit_map_index])&(1<<shift_index)))) {
			
			//printf("block %d aLlocated!\n",block);
			return 1;
	}
	else{
			
		//printf("%d Not allocated \n",block);
		
		return 0;
	}

}


void checkBitMap(int inode)
{

	struct ext2_inode* current=NULL;
	char buf[BASE_OFFSET*20];
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int sector=0;
	unsigned int dsector=0;
	unsigned int offset;
	unsigned int block=0;
    unsigned int dots=0;
	unsigned int i=0;
	unsigned int in_buf=0;	
	//static unsigned link_count=0;
	//static int level=1;
	char file_name[EXT2_NAME_LEN+1];
	
//	static unsigned int* hash_map;

	current=read_inode(inode);

	dir=malloc(sizeof(struct ext2_dir_entry_2));
	for(;current->i_block[i]!=0;i++){
#if 1 
		in_buf=0;	
		block=current->i_block[i];
		block=BLOCK_OFFSET(block);
		dsector=block/sector_size_bytes;
		dsector+=start_sector;
		printf("Block[i]: %d\n",current->i_block[i]);	
		if(checkBlockAllocated(current->i_block[i])){
				
			//	printf("Yes allocated\n");		


		}
		else{

				printf("No block %d not allocated\n",current->i_block[i]);
		}	
		if(dir==NULL) return ;
		read_device(device,dsector,2,buf);
		memcpy(dir,buf,sizeof(struct ext2_dir_entry_2));	
//		printf(" INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		block=dir->rec_len;		
#endif
	
#if 1		
	while(dir->inode!=0) {
//	while(block < current->i_size) 
#if 1
//			hash_map[dir->inode]++;
		if( dir->file_type==2 ) {

			if(!strcmp(dir->name,".") || !strcmp(dir->name,"..")) {
				//dots++;	
		//		if(!strcmp(dir->name,".")) {
				//	p=dir->inode;
	//				printf("%s\n",dir->name);
	//				checkItself(dir,n);
					
	//			}
			//	else {
				
			//memcpy(file_name,dir->name,dir->name_len);
			//file_name[dir->name_len]='\0';
		//	printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		//	dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d  parent is %d\n",dir->inode,file_name,dir->rec_len,dir->file_type,p);
			//		checkParent(dir,p);	
						
				//	p=dir->inode;
			//	}
			}		
			else { 
			memcpy(file_name,dir->name,dir->name_len);
			file_name[dir->name_len]='\0';
//			printf("%s/",file_name);
//			printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
			checkBitMap(dir->inode);	
//			printf("\n");
			}
#endif
		//	if(do_copy){
		//		printf("-----DO COPY!!!!------------\n");
#if 1 	//	
		//		memcpy((buf+in_buf),dir,sizeof(struct ext2_dir_entry_2));
		//		write_sectors(dsector,2,(buf));
#endif		
		//		do_copy=0;
		//	}
	
	}	
		memcpy(dir,(buf+block),sizeof(struct ext2_dir_entry_2));
		in_buf+=block;
		block+=dir->rec_len;		
	}
	
#endif	
    }
 



}

void clearHashMap(unsigned int* hash_map)
{
	int i=0;
	for(i=0;i<=total_inodes;i++)
		hash_map[i]=0;
	
}
void checkLinkCount(int inode)
{
	int i=0;
	struct ext2_inode* current=NULL;
	for(i=2;i<super->s_inodes_count;i++)
	{
		current=read_inode(i);
		if(current->i_links_count!=my_hash_map[i]) {
				
			printf("Changing the link count for %d from %d to %d\n",i,current->i_links_count,my_hash_map[i]);
			current->i_links_count=my_hash_map[i];	
			write_inode(i,current);
	
		}
			free(current);
	}
#if 0	
	struct ext2_inode* current=NULL;
	char buf[BASE_OFFSET*20];
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int sector=0;
	unsigned int dsector=0;
	unsigned int offset;
	unsigned int block=0;
    unsigned int dots=0;
	unsigned int i=0;
	unsigned int in_buf=0;	
	static unsigned link_count=0;
	static int level=1;
	char file_name[EXT2_NAME_LEN+1];
	static unsigned int* hash_map;
	//current=malloc(sizeof(struct ext2_inode));
	current=read_inode(inode);
    
	if(!link_count)	{
		hash_map=calloc((total_inodes+1),sizeof(unsigned int));
		clearHashMap(hash_map);
	}
	if(current==NULL) {
		//print_inode(current);
		return;
	}
//	print_inode(current);
	link_count++;
	dir=malloc(sizeof(struct ext2_dir_entry_2));
	//printf("INODE %d links count %d\n",inode,current->i_links_count);
	//printf("%d\n",6/8);
	for(;current->i_block[i]!=0;i++){
#if 1 
		in_buf=0;	
		block=current->i_block[i];
		block=BLOCK_OFFSET(block);
		dsector=block/sector_size_bytes;
		dsector+=start_sector;
		if(dir==NULL) return ;
		read_device(device,dsector,2,buf);
		memcpy(dir,buf,sizeof(struct ext2_dir_entry_2));	
//		printf(" INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		block=dir->rec_len;		
#endif
	
#if 1		
	while(dir->inode!=0) {
//	while(in_buf < block_size) { 
#if 1
			hash_map[dir->inode]++;
		if( dir->file_type==2 ) {

			if(!strcmp(dir->name,".") || !strcmp(dir->name,"..")) {
				//dots++;	
		//		if(!strcmp(dir->name,".")) {
				//	p=dir->inode;
	//				printf("%s\n",dir->name);
	//				checkItself(dir,n);
					
	//			}
			//	else {
				
			//memcpy(file_name,dir->name,dir->name_len);
			//file_name[dir->name_len]='\0';
		//	printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		//	dbg_p("INODE Number is %d Name is: %s Rec %d dir_type %d  parent is %d\n",dir->inode,file_name,dir->rec_len,dir->file_type,p);
			//		checkParent(dir,p);	
						
				//	p=dir->inode;
			//	}
			}		
			else { 
			
//			printf("INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
			checkLinkCount(dir->inode);	
			//printf("\n");
			}
#endif
		//	if(do_copy){
		//		printf("-----DO COPY!!!!------------\n");
#if 1 	//	
		//		memcpy((buf+in_buf),dir,sizeof(struct ext2_dir_entry_2));
		//		write_sectors(dsector,2,(buf));
#endif		
		//		do_copy=0;
		//	}
	
	}	
		memcpy(dir,(buf+block),sizeof(struct ext2_dir_entry_2));
		in_buf+=block;
		block+=dir->rec_len;		
	}
	
#endif	
    }
 
//		return current;
	free(dir);
	free(current);
	link_count--;
	if(link_count==0)	
		checkHash(hash_map);
#endif

}
void checkHash(unsigned int* hash_map)
{
	int i=ROOT;
	struct ext2_inode* current=NULL;
	for(i=2;i <= total_inodes;i++)
	{
		
		current=read_inode(i);
		if(current->i_links_count == hash_map[i]) {
				
				free(current);
			
		}else{
				printf("Link count for %d doesnt match should be %u and is %d. Changing.\n",i,hash_map[i],current->i_links_count);
				current->i_links_count=hash_map[i];
				write_inode(i,current);
				free(current);	
		}
		
		


	}


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
/*  ADD  '.' and '..' */
void addCP(int n,int c,int p)
{
	struct ext2_inode* current=NULL;
	char buf[BASE_OFFSET];
	struct ext2_dir_entry_2* dir=NULL;
	unsigned int dsector=0;
	unsigned int block=0;
	unsigned int i=0;
	unsigned in_buf=0;
	current=read_inode(n);
	
	if(current==NULL) {
		
		printf("%s: No allocation of c&p",__func__);
		return;	

	}

	//print_inode(current);	
	//printf("Size in bytes %u pointer to first data block %u blocks count %d links count %d\n",current->i_size,current->i_block[0],current->i_blocks,current->i_links_count);
	//printf("%d\n",6/8);
	for(;current->i_block[i]!=0;i++){
 
		block=current->i_block[i];
		block=BLOCK_OFFSET(block);
		dsector=block/sector_size_bytes;
		dsector+=start_sector;
//	printf("The sector is %d\n",sector);
		dir=malloc(sizeof(struct ext2_dir_entry_2));
		if(dir==NULL) return;
		read_device(device,dsector,2,buf);
		memcpy(dir,buf,sizeof(struct ext2_dir_entry_2));	
//		printf(" INODE Number is %d Name is: %s Rec %d dir_type %d\n",dir->inode,dir->name,dir->rec_len,dir->file_type);
		block=dir->rec_len;		
		in_buf=dir->rec_len;
		dir->inode=n;
		memcpy(buf,dir,sizeof(struct ext2_dir_entry_2));
		memcpy(dir,buf+dir->rec_len,sizeof(struct ext2_dir_entry_2));			
		dir->inode=p;
		memcpy(buf+in_buf,dir,sizeof(struct ext2_dir_entry_2));
		write_sectors(dsector,2,buf);
		free(current);
		free(dir);
		break;
	} 

	return;

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
	//void* buf=calloc(1024,sizeof(char));
	
	lseek(device,BLOCK_OFFSET(block)+start_sector*sector_size_bytes,SEEK_SET);
	write(device,buf,block_size);

	//return buf;

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
	//struct ext2_inode* current=malloc(sizeof(struct ext2_inode));
	unsigned int go_to=0;
	
	go_to=BLOCK_OFFSET(group_desc[group_no].bg_inode_table)+(start_sector*sector_size_bytes)+inode_off*inode_size;
	lseek64(device,go_to, SEEK_SET);

	write(device, i, sizeof(struct ext2_inode));
	
}
