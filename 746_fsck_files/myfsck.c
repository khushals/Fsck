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
	int partition=0;	
	int sector=0;
 	int f_part;	
	/*Get the arguments from command line*/	
	fillArgs(argc,argv,&partition,&f_part,&disk_image);		

	if( (device = open(disk_image,O_RDWR)) == -1) {
		
		perror("Could not open the device file");
		exit(-1);
	}
	/*get the partition entry*/
	if(partition!=0)
		partitionTableEntry(partition,sector);
	else
		checkFS(f_part);
	return 0;
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
		temp->p.start_sect +=offset;
		addToList(head,temp);	
	}
		checkForExtended(head);
		

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

/* checkFS- To Process 'f' option 
	f --> 0 Check all.
	f--> Check partition number	
*/

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
						
					case 3:
						checkLinkCount(ROOT);
						
					case 4:
						checkBlockBitMap(ROOT);
						crossBlockCheck();	
					default:
							pass=4;
							break;


			}

		}


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
				//printf("%s:INODE Number is %d Name is: %s Rec %d dir_type %d\n",__func__,dir->inode,file_name,dir->rec_len,dir->file_type);
				
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
			my_inode_map[dir->inode]++;	
			
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
void addToLostAndFound(int inode)
{

	unsigned char* buf;
	//char* buf[];
	int i=0;
	int block=0;
	struct ext2_dir_entry_2* dir;
	int get_block=getBlockForDirectory(inode);	
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

void checkLinkCount(int inode)
{
	int i=0;
	struct ext2_inode* current=NULL;
	for(i=2;i<super->s_inodes_count;i++)
	{
		current=read_inode(i);
		if(current->i_links_count!=my_inode_map[i]) {
				
			printf("Changing the link count for %d from %d to %d\n",i,current->i_links_count,my_inode_map[i]);
			current->i_links_count=my_inode_map[i];	
			write_inode(i,current);
	
		}
			free(current);
	}


}
