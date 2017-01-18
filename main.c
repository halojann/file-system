#include <stdio.h>
#include <stdlib.h>

#define blocks_total 16384
#define blocks_superblock 2
#define blocks_inode (blocks_total - blocks_superblock)/136
#define blocks_data (blocks_total-blocks_superblock-blocks_inode)

#define inode_index blocks_superblock
#define data_index inode_index + blocks_inode
#define url_file "/home/halojann/workspace/SSOS final project/File System/Drive10MB"

struct block{

    int block_num;//block index
    char block_type;// d or s or i
    char allocated;//f if free , a if allocated , w if set to wipe
    char content[503];

};

struct infoblock{

    int block_num;//block index
    char block_type;// d or s or i
    char allocated;//f if free , a if allocated , w if set to wipe

    int sblocks_total;
    int sblocks_superblock;
    int sblocks_inode;
    int sblocks_data;
    int sinode_index;
    int sdata_index;
    char surl_file[128];

    int last_free;
    int free_dblocks;
    int free_inodes;

    int total_dir;
    int total_files;

    char content[330];

};

struct dir_entry{

    char name[64];
    int inode;
};

struct dir{//just directories

    int block_num;//block index
    char block_type;// d or s or i
    char allocated;//f if free , a if allocated , w if set to wipe

    char name[64];//directory name
    int inode_id;//holds the block_num of its own inode
    int size;//number of entries

    struct dir_entry entries[6];
    char content[24];
};

struct file{

    int block_num;//block index
    char block_type;// d or s or i
    char allocated;//f if free , a if allocated , w if set to wipe
    int empty;

    char file_name[64];//<filename>.<xyz> format

    int inode_id;//holds the block_num of its own inode

    char content[432];//file contents
};

struct inode{//properties common to both files and folders

    int block_num;
    char block_type;
    char allocated;

    int size;//number of blocks allocated
    unsigned long time_of_creation;//10 digit epoch time
    unsigned long last_modified;
    unsigned long last_accessed;

    int datablocks[10];//holds block_num of allocated data blocks
    int parent_inode;//block_num of parent inode
    int total_char;
    char content[418];

};

struct indirect_block{//contains 292 blocks which point to data blocks

    int block_num;
    char block_type;//indirect to have block type a
    char allocated;

    int size;//number of blocks allocated
    int datablocks[125];//holds block_num of allocated data blocks

};

struct entry{
    int inode;
    int directory;
};

int format(){

    printf("Formatting...\n");
    FILE * fp = fopen(url_file,"wb");//opening file in binary mode - write

// Initial format - blocks only
    printf("Writing %d blocks\n",blocks_total);

    int i=0,d=0;
    for(;i<blocks_total;i++){

    if(i==0){//superblock at index 0 will remain an infoblock

            struct infoblock *b = malloc(sizeof(struct infoblock));
            b->block_num = i;
            b->allocated='a';
            b->block_type='s';
            b->sblocks_total=blocks_total;
            b->sblocks_superblock=blocks_superblock;
            b->sblocks_inode=blocks_inode;
            b->sblocks_data=blocks_data;
            b->sinode_index=inode_index;
            b->sdata_index=data_index;
            b->last_free=data_index+1;//first datablock will be alloted for the root directory
            b->free_dblocks=blocks_data-1;
            b->free_inodes=blocks_inode;
            b->total_dir=1;
            b->total_files=0;
            strncpy(b->surl_file,url_file,128);

            fwrite(b,sizeof(struct infoblock),1,fp);
            memset(b,0,sizeof(struct infoblock));
            free(b);
    }
    else if(i==1){//superblock at index 1 will be the root directory inode

            struct inode *root = malloc(sizeof(struct inode));

            root->allocated = 'a';
            root->block_num = 1;
            root->block_type= 's';
            root->datablocks[0]= data_index;//free after formatting - first data block alloted
            root->parent_inode= -1;
            root->size= 1;
            root->time_of_creation= (unsigned long)(time(NULL));
            root->last_accessed= (unsigned long)(time(NULL));
            root->last_modified= (unsigned long)(time(NULL));

            fwrite(root,sizeof(struct inode),1,fp);
            memset(root,0,sizeof(struct inode));
            free(root);
    }
    else if(i<blocks_inode+blocks_superblock){//inode table
            struct inode *in = malloc(sizeof(struct inode));
            in->block_type='i';
            in->block_num = i;
            in->allocated='f';
            fwrite(in,sizeof(struct inode),1,fp);
            memset(in,0,sizeof(struct inode));
            free(in);
    }
    else if(i<blocks_total){//data blocks

        if(d==0){//data block for the root directory

                struct dir *rd = malloc(sizeof(struct dir));

                rd->block_num = i;
                rd->allocated = 'a';
                rd->block_type= 'f';
                rd->size = 0;
                rd->inode_id=1;
                strncpy(rd->name,"",64);
                fwrite(rd,sizeof(struct dir),1,fp);
                memset(rd,0,sizeof(struct dir));
                free(rd);

                d=1;
        }
        else{
                struct block *d = malloc(sizeof(struct block));
                d->block_num = i;
                d->allocated='f';
                d->block_type='d';
                fwrite(d,sizeof(struct block),1,fp);
                memset(d,0,sizeof(struct block));
                free(d);
            }
    }
}

//creating root directory inode and corresponding data block

    printf("Block 0 of superblock -> Infoblock\n");
    printf("Block 1 of superblock -> Root inode\n");
    printf("First data block -> Root directory\n");

    fclose(fp);//closing file

    return 1;

}

int next_free_block(int auto_wipe){//returns the block index of the next free data block

    FILE * fp = fopen(url_file,"rb+");
    struct infoblock *ib = malloc(sizeof(struct infoblock));//reading filesystem details from infoblock
    fread(ib,sizeof(struct infoblock),1,fp);
    int current_index = ib->last_free;
    int first = current_index;

    rewind(fp);
    fseek(fp,512*current_index,SEEK_SET);//seek to last free data index

    while(current_index<ib->sblocks_total){

        struct block *b = malloc(sizeof(struct block));
        fread(b,sizeof(struct block),1,fp);

        if(b->allocated=='f'){
            int index=b->block_num;
            ib->last_free = index;
            fseek(fp,0,SEEK_SET);//rewind again to update infoblock
            fwrite(ib,sizeof(struct infoblock),1,fp);
            free(b);
            free(ib);
            fclose(fp);
            return(index);//return block index of free data block
        }
        current_index++;
        free(b);
    }

    //search again from start till previous start point
    current_index = ib->sdata_index;
    fseek(fp,0,SEEK_SET);
    fseek(fp,512*(current_index-1),SEEK_SET);
    while(current_index<=first){

        struct block *b = malloc(sizeof(struct block));
        fread(b,sizeof(struct block),1,fp);
        if(b->allocated=='f'){
            int index=b->block_num;
            ib->last_free = index;
            fseek(fp,0,SEEK_SET);//rewind again to update infoblock
            fwrite(ib,sizeof(struct infoblock),1,fp);
            free(b);
            free(ib);
            fclose(fp);
            return(index);//return block index of free data block
        }
        current_index++;
        free(b);
    }
    //no free block

    fclose(fp);
    free(ib);

    if(auto_wipe==1){
        wipe();
        return next_free_block(0);
    }

    return(-1);
}

int next_free_inode(int auto_wipe){

    FILE * fp = fopen(url_file,"rb");
    struct infoblock *ib = malloc(sizeof(struct infoblock));//reading filesystem details from infoblock
    fread(ib,sizeof(struct infoblock),1,fp);

    rewind(fp);
    fseek(fp,512*ib->sinode_index,SEEK_SET);//seek to first inode in the table

    int i=0;
    int index;

    while(i<ib->sblocks_inode){

        struct inode *b = malloc(sizeof(struct inode));
        fread(b,sizeof(struct inode),1,fp);

        if(b->allocated =='f'){
            index = b->block_num;
            free(b);
            free(ib);
            fclose(fp);
            return(index);//return block index
        }
        i++;
        free(b);
    }
    free(ib);
    fclose(fp);

        if(auto_wipe==1){
        wipe();
        return next_free_inode(0);
        }

    return -1;
    }

int mount(){

    FILE * fp = fopen(url_file,"rb");
    struct infoblock *ib = malloc(sizeof(struct infoblock));//reading filesystem details from infoblock
    fread(ib,sizeof(struct infoblock),1,fp);
    printf("\n-------------------Mount successful-------------\n");
    printf("Number of blocks : %d\n",ib->sblocks_total);
    printf("Number of superblocks : %d\n",ib->sblocks_superblock);
    printf("Number of inodes : %d\n",ib->sblocks_inode);
    printf("Number of data blocks : %d\n",ib->sblocks_data);
    printf("Inodes start at block index : %d\n",ib->sinode_index);
    printf("Data blocks start at block index : %d\n",ib->sdata_index);
    printf("Free data block last found at index : %d\n",ib->last_free);
    printf("Current File system url : %s\n",ib->surl_file);
    printf("Number of free data blocks : %d\n",ib->free_dblocks);
    printf("Data blocks in use: %d\n",ib->sblocks_data-ib->free_dblocks);
    printf("Number of free inode blocks : %d\n",ib->free_inodes);
    printf("Inodes in use : %d\n",ib->sblocks_inode-ib->free_inodes);
    printf("Number of files : %d\n",ib->total_files);
    printf("Number of directories : %d\n",ib->total_dir);
    free(ib);
    fclose(fp);
    return 1;

}

void wipe_block(int block_num,char type){//wipe a single block

    FILE * fp = fopen(url_file,"rb+");
    fseek(fp,512*block_num,SEEK_SET);

    if(type=='i'){
            struct inode *in = malloc(sizeof(struct inode));
            in->block_type='i';
            in->block_num = block_num;
            in->allocated='f';
            fwrite(in,sizeof(struct inode),1,fp);
            free(in);
    }
    if(type=='d'){

            struct block *d = malloc(sizeof(struct block));
            d->block_num = block_num;
            d->allocated='f';
            d->block_type='d';
            fwrite(d,sizeof(struct block),1,fp);
            free(d);
    }

    fclose(fp);
}

int wipe(){

    FILE * fp = fopen(url_file,"rb+");

    struct infoblock *ib = malloc(sizeof(struct infoblock));//reading filesystem details from infoblock
    fread(ib,sizeof(struct infoblock),1,fp);

    fseek(fp,512*ib->sinode_index-1,SEEK_SET);//seek to first inode block
    int current_index = ib->sinode_index;
    int blocks_wiped = 0;
    int inodes_wiped = 0;

    for(;current_index<ib->sdata_index;current_index++){

        struct inode *in = malloc(sizeof(struct inode));
        fread(in,sizeof(struct inode),1,fp);
        if(in->allocated=='w'){

        int i=0;
        while(i<in->size){//erasing its 9 data-blocks + 1 indirect block

            if(in->datablocks[i]!=0){//direct blocks
                wipe_block(in->datablocks[i],'d');
                blocks_wiped++;
            }

            if((i==9)&&(in->datablocks[i]!=0)){//indirect block
            int j=0;

            struct indirect_block *indirect = malloc(sizeof(struct indirect_block));//reading indirect block
            rewind(fp);
            fseek(fp,512*in->datablocks[9],SEEK_SET);
            fread(indirect,sizeof(struct indirect_block),1,fp);

            while(j<indirect->size){//wiping data-blocks listed

                if(indirect->datablocks[j]!=0){
                        wipe_block(indirect->datablocks[j],'d');
                        blocks_wiped++;
                }
                j++;
            }
            }//done with indirect

        i++;
        }//done with inode
        wipe_block(in->block_num,'i');
        inodes_wiped++;
        }
    }


    rewind(fp);
    fseek(fp,512*ib->sdata_index-1,SEEK_SET);//seek to first data block
    current_index = ib->sdata_index;
    blocks_wiped =0;

    for(;current_index<ib->sblocks_total;current_index++){

        struct block *b = malloc(sizeof(struct block));
        fread(b,sizeof(struct block),1,fp);

        if(b->allocated=='w'){
            wipe_block(b->block_num,'d');
            blocks_wiped++;
            }
        free(b);
    }

    ib->free_dblocks = ib->free_dblocks + blocks_wiped;//updating free block counts
    ib->free_inodes = ib->free_inodes + inodes_wiped;
    fseek(fp,0,SEEK_SET);
    fwrite(ib,sizeof(struct infoblock),1,fp);


    printf("%d data blocks wiped\n",blocks_wiped);
    printf("%d inodes wiped\n",inodes_wiped);
    free(ib);
    fclose(fp);
    return 1;

}

int verify(){

    FILE * fp = fopen(url_file,"rb");
    struct block *b = malloc(sizeof(struct block));
    int i,ns=0,ni=0,nd=0,allo=0;
    int ui=0,ud=0,in=0,nf=0,ne=0;

    printf("\n------------------CURRENT BLOCK COUNTS----------\n");

    for(i=0;i<blocks_total;i++){

    fread(b,sizeof(struct block),1,fp);
    if(b->allocated=='a')allo++;

    switch(b->block_type){

        case 's':
        ns++;
        break;

        case 'i':
        ni++;
        if(b->allocated=='a')ui++;
        break;

        case 'd':
        nd++;
        if(b->allocated=='a')ud++;
        break;

        case 'a':
        in++;
        nd++;
        ud++;
        break;

        case 'f':
        nf++;
        nd++;
        ud++;
        break;

        case 'e':
        ne++;
        nd++;
        ud++;
        break;

    }
    }
    printf("Superblocks: %d , Inodes %d , Data blocks %d \n",ns,ni,nd);
    printf("Total allocated:  %d \n",allo);
    printf("Allocated Inodes %d , Data blocks %d \n",ui,ud);
    if(in>0)printf("Indirect blocks : %d\n",in);
    if(nf>0)printf("Directories created : %d\n",nf);
    if(ne>0)printf("Files created : %d\n",ne);

    free(b);
    fclose(fp);
    return 1;
}

int check_space(int size,int auto_wipe){

    FILE * fp = fopen(url_file,"rb");
    struct infoblock *ib = malloc(sizeof(struct infoblock));//reading filesystem details from infoblock
    fread(ib,sizeof(struct infoblock),1,fp);
    fseek(fp,512*(ib->sdata_index),SEEK_SET);//seek to first data block

    int available = 0,i=0;
    size = size/512;

    for(;i<ib->sblocks_data;i++){

    struct block *b = malloc(sizeof(struct block));
    fread(b,sizeof(struct block),1,fp);

    if(b->allocated =='f'){
    available++;
    }

    if(available==size){
    printf("Space available\n");
    free(b);
    free(ib);
    fclose(fp);

    return 1;
    }

    free(b);
    }

    free(ib);
    fclose(fp);

    if(auto_wipe==1){
        wipe();
        if(check_space(size,0))return 1;
    }
    return 0;
}

struct entry get_existing_inode(char *path){

    const char delimiter[2] = "/";
    char *copy = strdup(path);
    char *seg = strtok(copy,delimiter);

    struct inode *in = malloc(sizeof(struct inode));
    struct dir *d = malloc(sizeof(struct dir));

    FILE * fp = fopen(url_file,"rb");
    fseek(fp,512,SEEK_SET);//Navigate to root inode
    fread(in,sizeof(struct inode),1,fp);
    rewind(fp);
    fseek(fp,512*(in->datablocks[0]),SEEK_SET);//Navigate to root directory
    fread(d,sizeof(struct dir),1,fp);

    while(seg != NULL){//navigate to final directory
        int i,flag=0;
        for(i=0;i<d->size;i++){

            if(!strcmp(seg,d->entries[i].name)){//match case
                flag=1;
                rewind(fp);
                fseek(fp,512*(d->entries[i].inode),SEEK_SET);//seek to directory inode
                fread(in,sizeof(struct inode),1,fp);
                rewind(fp);
                fseek(fp,512*(in->datablocks[0]),SEEK_SET);//seek to directory datablock
                fread(d,sizeof(struct dir),1,fp);
                break;
            }
        }
        //invalid path - folder not found
        if(flag==0){
        printf("Path error: folder not found\n");
        free(d);
        free(in);
        return;
        }
        seg = strtok(NULL,delimiter);
    }

    struct entry *e = malloc(sizeof(struct entry));
    e->directory = d->block_num;
    e->inode = in->block_num;

    free(in);
    free(d);
    return *e;
}

int makedir(char* path,char* d_name){

    struct inode *in = malloc(sizeof(struct inode));
    struct dir *d = malloc(sizeof(struct dir));
    FILE * fp = fopen(url_file,"rb+");

    struct entry e= get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path
    char *nm = strdup(d_name);

    if(strlen(nm)>64){
        printf("Directory name cannot be more than 64 chars\n");
        return -1;
    }

    char invalid_chars[] = "!@#$%^&*()/,;'" ":-.=+\|[{]}`~?><";
    int flag=0;
    int i;

    for(i=0;i<strlen(invalid_chars);i++){
        if(strchr(nm,invalid_chars[i])!=NULL){
            flag=1;
            break;
        }
    }
    if(flag){
    printf("characters: !@#$%^&*()/,;'" ":-=+\|[{]}`~?>< not allowed in directory name\n");
    return -1;
    }

    fseek(fp,512*e.inode,SEEK_SET);
    fread(in,sizeof(struct inode),1,fp);
    rewind(fp);
    fseek(fp,512*e.directory,SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);
    //create new directory

    rewind(fp);
    int inode = next_free_inode(1);
    int block = next_free_block(1);

    if((inode==-1)||(block==-1)){//resource check

    printf("Cant create directory :");
    if(inode==-1)printf("No free inodes\n");
    if(block==-1)printf("No free data-blocks\n");
    free(d);
    free(in);

    return -1;
    }

    //duplicate entry check
    i=0;

    for(;i<d->size;i++){

        if(strcmp(nm,d->entries[i].name)==0){
            printf("Cant create directory : Directory already exists\n");
            free(d);
            free(in);
            return -1;
        }
    }

    struct inode *node = malloc(sizeof(struct inode));//creating directory inode
    node->allocated = 'a';
    node->block_num = inode;
    node->block_type= 'i';
    node->parent_inode = in->block_num;
    node->size = 1;
    node->time_of_creation= (unsigned long)(time(NULL));
    node->last_accessed= (unsigned long)(time(NULL));
    node->last_modified= (unsigned long)(time(NULL));
    node->datablocks[0]= block;

    fseek(fp,512*inode,SEEK_SET);
    fwrite(node,sizeof(struct inode),1,fp);
    rewind(fp);
    fseek(fp,512*block,SEEK_SET);

    struct dir *f = malloc(sizeof(struct dir));//creating directory datablock
    f->allocated = 'a';
    f->block_num = block;
    f->block_type= 'f';
    f->inode_id = inode;
    strncpy(f->name,d_name,64);
    f->size = 0;
    fwrite(f,sizeof(struct dir),1,fp);

    //updating parent directory

    strncpy(d->entries[d->size].name,d_name,64);
    d->entries[d->size].inode = inode;
    d->size++;

    rewind(fp);
    fseek(fp,512*d->block_num,SEEK_SET);
    fwrite(d,sizeof(struct dir),1,fp);
    fclose(fp);

    free(d);
    free(in);
    printf("Directory %s : created\n",d_name);
    return 1;

}

int readdir(char* path){

    struct dir *d = malloc(sizeof(struct dir));
    FILE * fp = fopen(url_file,"rb");

    struct entry e= get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    fseek(fp,512*e.directory,SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);
    printf("Directory contains %d item(s):\n",d->size);
    //read directory contents
    int i;
    for(i=0;i<d->size;i++){
        printf("%s \n",d->entries[i].name);
    }

    fclose(fp);
    free(d);
    return 1;

}

int renamedir(char* path,char* name){

    FILE * fp = fopen(url_file,"rb+");

    struct entry e= get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    struct inode *in = malloc(sizeof(struct inode));
    struct dir *d = malloc(sizeof(struct dir));

    fseek(fp,512*e.inode,SEEK_SET);
    fread(in,sizeof(struct inode),1,fp);

    rewind(fp);
    fseek(fp,512*e.directory,SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);
    //rename directory
    rewind(fp);
    fseek(fp,512*e.directory,SEEK_SET);
    char *copy = strdup(d->name);
    strncpy(d->name,name,64);
    fwrite(d,sizeof(struct dir),1,fp);
    //updating parent directory
    rewind(fp);
    fseek(fp,512*in->parent_inode,SEEK_SET);
    fread(in,sizeof(struct inode),1,fp);
    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);//parent directory datablock
    int i=0;
    for(;i<d->size;i++){
        if(strcmp(d->entries[i].name,copy)==0){
            strncpy(d->entries[i].name,name,64);
            break;
        }
    }
    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    fwrite(d,sizeof(struct dir),1,fp);

    fclose(fp);
    free(d);
    free(in);
    printf("Directory %s renamed\n",name);
    return 1;

}

int create(char* path,char *filename){

    FILE * fp = fopen(url_file,"rb+");

    char *fname = strdup(filename);
    const char delim[2] = ".";
    char *name = strtok(fname,delim);
    char *ext = strtok(NULL,delim);

    if((strlen(name)>60)||(strlen(ext)>3)){
        printf("File name should have maximum 64 chars out of which the extension has 3 \n");
        return -1;
    }
    if(strtok(NULL,delim)!=NULL){
        printf("File name should not have more than one '.' character\n");
        return -1;
    }

    char invalid_chars[] = "!@#$%^&*()/,;'" ":-=+\|[{]}`~?><";
    int flag=0;
    int i;

    for(i=0;i<strlen(invalid_chars);i++){
        if(strchr(filename,invalid_chars[i])!=NULL){
            flag=1;
            break;
        }
    }
    if(flag){
        printf("characters: !@#$^%&*()/,;'" ":-=+\|[{]}`~?>< not allowed in file name\n");
        return -1;
    }

    struct entry e = get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    struct inode *in = malloc(sizeof(struct inode));
    struct dir *d = malloc(sizeof(struct dir));
    struct file *f = malloc(sizeof(struct file));

    int ino = next_free_inode(1);
    int blo = next_free_block(1);

    fseek(fp,512*e.inode,SEEK_SET);//reading and updating parent directory
    fread(in,sizeof(struct inode),1,fp);
    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);

        for(i=0;i<d->size;i++){//duplicate file check

        if(strcmp(filename,d->entries[i].name)==0){
            printf("Cant create File : File already exists\n");
            free(d);
            free(in);
            free(f);
            fclose(fp);
            return -1;
        }
    }


    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    strncpy(d->entries[d->size].name,filename,64);
    d->entries[d->size].inode=ino;
    d->size++;
    fwrite(d,sizeof(struct dir),1,fp);

    rewind(fp);//creating new inode for file
    fseek(fp,512*ino,SEEK_SET);
    in->block_num=ino;
    in->block_type='i';
    in->datablocks[0]=blo;
    in->time_of_creation= (unsigned long)(time(NULL));
    in->last_accessed= (unsigned long)(time(NULL));
    in->last_modified= (unsigned long)(time(NULL));
    in->parent_inode=e.inode;
    in->size=1;
    fwrite(in,sizeof(struct dir),1,fp);

    rewind(fp);//creating new file struct
    fseek(fp,512*blo,SEEK_SET);
    f->allocated='a';
    f->block_num=blo;
    f->block_type='e';
    strncpy(f->file_name,filename,64);
    f->inode_id=ino;
    f->empty=1;
    fwrite(f,sizeof(struct dir),1,fp);
    printf("File: %s : created\n",filename);
    fclose(fp);
    free(f);
    free(d);
    free(in);
    return 1;
}

int writefile(char* path,char *filename,char* string){

    struct entry e = get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    FILE * fp = fopen(url_file,"rb+");
    struct dir *d = malloc(sizeof(struct dir));
    fseek(fp,512*e.directory,SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);//get directory

    int i;
    for(i=0;i<d->size;i++){
        if(strcmp(d->entries[i].name,filename)==0){
            break;
        }
    }
    rewind(fp);
    fseek(fp,512*d->entries[i].inode,SEEK_SET);
    struct inode *in = malloc(sizeof(struct inode));
    fread(in,sizeof(struct inode),1,fp);//get inode
    in->total_char = strlen(string);
    fseek(fp,-512,SEEK_CUR);
    fwrite(in,sizeof(struct inode),1,fp);

    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    struct file *f = malloc(sizeof(struct file));
    fread(f,sizeof(struct file),1,fp);//get file struct

    if(f->empty==0){//wipe inode contents if file has been written before

    in->size=1;
    rewind(fp);
    fseek(fp,512*in->block_num,SEEK_SET);
    fwrite(in,sizeof(struct inode),1,fp);

    }
    int len = strlen(string);

    strncpy(f->content,string,432);//copy first 432 chars to file struct itself
    f->empty=0;
    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    fwrite(f,sizeof(struct file),1,fp);//write to file struct
    len=len-432;
    string += 432;
    free(f);

    while(len>0){//write remaining in new datablocks/indirect blocks until finished

    if(in->size<9){//direct block
        in->datablocks[in->size]=next_free_block(1);//add new block in inode
        in->size++;
        rewind(fp);
        fseek(fp,512*in->block_num,SEEK_SET);
        fwrite(in,sizeof(struct inode),1,fp);//update inode with new direct block

        struct block *b=malloc(sizeof(struct block));//direct block
        memset(b,' ',sizeof(struct block));
        b->allocated='a';
        b->block_num=next_free_block(1);
        strncpy(b->content,string,503);
        b->block_type='d';
        rewind(fp);
        fseek(fp,512*next_free_block(1),SEEK_SET);
        fwrite(b,sizeof(struct block),1,fp);
        string +=503;
        len -= 503;
        free(b);
    }
    else{//indirect block

                int index = in->size-9;
                struct indirect_block *ind= malloc(sizeof(struct indirect_block));
                in->size++;

                if(index>0){//not the first indirect block data block
                rewind(fp);
                fseek(fp,512*in->datablocks[9],SEEK_SET);
                fread(ind,sizeof(struct indirect_block),1,fp);//get indirect block
                ind->datablocks[ind->size]=next_free_block(1);
                ind->size++;
                fseek(fp,-512,SEEK_CUR);
                fwrite(ind,sizeof(struct indirect_block),1,fp);//update indirect block

                struct block *b=malloc(sizeof(struct block));
                memset(b,32,sizeof(struct block));
                b->allocated='a';
                b->block_num=next_free_block(1);
                strncpy(b->content,string,503);
                b->block_type='d';
                rewind(fp);
                fseek(fp,512*next_free_block(1),SEEK_SET);
                fwrite(b,sizeof(struct block),1,fp);//write next 503 characters
                string +=503;
                len -= 503;
                free(b);
                }
            else{//first indirect block
                int nfb = next_free_block(1);
                in->datablocks[9]=nfb;
                ind->allocated='a';
                ind->block_num=nfb;
                ind->size=1;
                ind->block_type='a';
                rewind(fp);
                fseek(fp,512*nfb,SEEK_SET);
                fwrite(ind,sizeof(struct indirect_block),1,fp);//create indirect block
                nfb = next_free_block(1);
                ind->datablocks[0]=nfb;
                fseek(fp,-512,SEEK_CUR);
                fwrite(ind,sizeof(struct indirect_block),1,fp);//write indirect block

                struct block *b=malloc(sizeof(struct block));
                memset(b,32,sizeof(struct block));
                b->allocated='a';
                b->block_num=nfb;
                strncpy(b->content,string,503);
                b->block_type='d';
                rewind(fp);
                fseek(fp,512*nfb,SEEK_SET);
                fwrite(b,sizeof(struct block),1,fp);//write data block
                string +=503;
                len -= 503;
                free(b);
                }

        rewind(fp);
        fseek(fp,512*in->block_num,SEEK_SET);
        fwrite(in,sizeof(struct inode),1,fp);//update inode size
        free(ind);
    }

        rewind(fp);
        fseek(fp,512*in->block_num,SEEK_SET);
        fread(in,sizeof(struct inode),1,fp);
    }
    printf("Written to file\n");

    free(d);
    free(in);
    fclose(fp);
    return 1;
}

char *readfile(char* path,char *filename){

    char* str = NULL;

    struct entry e = get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    FILE * fp = fopen(url_file,"rb");
    struct dir *d = malloc(sizeof(struct dir));
    fseek(fp,512*e.directory,SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);//get directory

    int i;
    for(i=0;i<d->size;i++){
        if(strcmp(d->entries[i].name,filename)==0){
            break;
        }
    }
    rewind(fp);
    fseek(fp,512*d->entries[i].inode,SEEK_SET);
    struct inode *in = malloc(sizeof(struct inode));
    fread(in,sizeof(struct inode),1,fp);//get inode
    int tot = in->total_char;
    str = malloc(tot*sizeof(char));

    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    struct file *f = malloc(sizeof(struct file));
    fread(f,sizeof(struct file),1,fp);//get file
    int k;
    if(f->empty==1){
        printf("Empty file\n");
        return -1;
    }else{
    k=0;
    int m;
    for(i=0;i<432;i++){
    if(k<tot){
    str[k]=f->content[i];
    k++;
    }}

    }

        for(i=1;i<in->size;i++){

            rewind(fp);
            fseek(fp,512*in->datablocks[i],SEEK_SET);

            if(i<9){
                struct block *bl = malloc(sizeof(struct block));
                fread(bl,sizeof(struct block),1,fp);
                int m;
                for(m=0;m<503;m++){
                if(k<tot){
                str[k]=bl->content[m];
                k++;
                }}
                memset(bl,' ',512);
                free(bl);
            }

            if(i==9){
                struct indirect_block *ibl = malloc(sizeof(struct indirect_block));
                fread(ibl,sizeof(struct indirect_block),1,fp);
                int j;
                for(j=0;j<ibl->size;j++){
                    rewind(fp);
                    fseek(fp,512*ibl->datablocks[j],SEEK_SET);

                    struct block *blo = malloc(sizeof(struct block));
                    fread(blo,sizeof(struct block),1,fp);
                    int m;
                    for(m=0;m<503;m++){
                    if(k<tot){
                    str[k]=blo->content[m];
                    k++;
                    }}

                    printf("\n");
                    free(blo);
                }
                free(ibl);
            }
        }

    free(f);
    free(d);
    free(in);
    fclose(fp);
    str[tot]='\0';
    return str;
}

int appendfile(char* path,char *filename,char* string){

    struct entry e = get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    FILE * fp = fopen(url_file,"rb+");
    struct dir *d = malloc(sizeof(struct dir));
    fseek(fp,512*e.directory,SEEK_SET);
    fread(d,sizeof(struct dir),1,fp);//get directory

    int i;
    for(i=0;i<d->size;i++){
        if(strcmp(d->entries[i].name,filename)==0){
            break;
        }
    }
    rewind(fp);
    fseek(fp,512*d->entries[i].inode,SEEK_SET);
    struct inode *in = malloc(sizeof(struct inode));
    fread(in,sizeof(struct inode),1,fp);//get inode
    in->total_char = in->total_char+strlen(string);
    fseek(fp,-512,SEEK_CUR);
    fwrite(in,sizeof(struct inode),1,fp);

    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    struct file *f = malloc(sizeof(struct file));
    fread(f,sizeof(struct file),1,fp);//get file struct
    int total;

    if(f->empty==1){

    total = 0;

    }else{

    total = in->total_char;

    }
    int len = strlen(string);
    if(total+len<433){

    strncpy(f->content,strncat(f->content,string,432),432);//copy first 432 chars to file struct itself
    f->empty=0;
    rewind(fp);
    fseek(fp,512*in->datablocks[0],SEEK_SET);
    fwrite(f,sizeof(struct file),1,fp);//write to file struct
    len=len-432;
    string += 432;
    free(f);
    }

    while(len>0){//write remaining in new datablocks/indirect blocks until finished

    if(in->size<9){//direct block

    if((total+len)<(432+(503*in->size)+1)){
        in->datablocks[in->size]=next_free_block(1);//add new block in inode
        in->size++;
        rewind(fp);
        fseek(fp,512*in->block_num,SEEK_SET);
        fwrite(in,sizeof(struct inode),1,fp);//update inode with new direct block

        struct block *b=malloc(sizeof(struct block));//direct block
        memset(b,' ',sizeof(struct block));
        b->allocated='a';
        b->block_num=next_free_block(1);
        strncpy(b->content,strncat(b->content,string,503),503);
        b->block_type='d';
        rewind(fp);
        fseek(fp,512*next_free_block(1),SEEK_SET);
        fwrite(b,sizeof(struct block),1,fp);
        string +=503;
        len -= 503;
        free(b);
    }}
    else{//indirect block
        if((total+len)<(432+(503*in->size)+1)){
                int index = in->size-9;
                struct indirect_block *ind= malloc(sizeof(struct indirect_block));
                in->size++;

                if(index>0){//not the first indirect block data block
                rewind(fp);
                fseek(fp,512*in->datablocks[9],SEEK_SET);
                fread(ind,sizeof(struct indirect_block),1,fp);//get indirect block
                ind->datablocks[ind->size]=next_free_block(1);
                ind->size++;
                fseek(fp,-512,SEEK_CUR);
                fwrite(ind,sizeof(struct indirect_block),1,fp);//update indirect block

                struct block *b=malloc(sizeof(struct block));
                memset(b,32,sizeof(struct block));
                b->allocated='a';
                b->block_num=next_free_block(1);
                strncpy(b->content,strncat(b->content,string,503),503);
                b->block_type='d';
                rewind(fp);
                fseek(fp,512*next_free_block(1),SEEK_SET);
                fwrite(b,sizeof(struct block),1,fp);//write next 503 characters
                string +=503;
                len -= 503;
                free(b);
                }
            else{//first indirect block
                int nfb = next_free_block(1);
                in->datablocks[9]=nfb;
                ind->allocated='a';
                ind->block_num=nfb;
                ind->size=1;
                ind->block_type='a';
                rewind(fp);
                fseek(fp,512*nfb,SEEK_SET);
                fwrite(ind,sizeof(struct indirect_block),1,fp);//create indirect block
                nfb = next_free_block(1);
                ind->datablocks[0]=nfb;
                fseek(fp,-512,SEEK_CUR);
                fwrite(ind,sizeof(struct indirect_block),1,fp);//write indirect block

                struct block *b=malloc(sizeof(struct block));
                memset(b,32,sizeof(struct block));
                b->allocated='a';
                b->block_num=nfb;
                strncpy(b->content,strncat(b->content,string,503),503);
                b->block_type='d';
                rewind(fp);
                fseek(fp,512*nfb,SEEK_SET);
                fwrite(b,sizeof(struct block),1,fp);//write data block
                string +=503;
                len -= 503;
                free(b);
                }

        rewind(fp);
        fseek(fp,512*in->block_num,SEEK_SET);
        fwrite(in,sizeof(struct inode),1,fp);//update inode size
        free(ind);
    }

        rewind(fp);
        fseek(fp,512*in->block_num,SEEK_SET);
        fread(in,sizeof(struct inode),1,fp);
    }}
    printf("File appended\n");

    free(d);
    free(in);
    fclose(fp);
    return 1;
}

int remove_entry(char* path,char* name){

    FILE * fp = fopen(url_file,"rb+");

    struct entry e= get_existing_inode(path);
    if(e.inode+e.directory==1)return -1;//invalid path

    struct inode *in = malloc(sizeof(struct inode));
    struct dir *d = malloc(sizeof(struct dir));

    fseek(fp,512*e.inode,SEEK_SET);
    fread(in,sizeof(struct inode),1,fp);//get parent inode

    rewind(fp);
    fseek(fp,512*e.directory,SEEK_SET);//get parent directory
    fread(d,sizeof(struct dir),1,fp);

    int target_inode;
    char *childpath = strdup(path);
    //updating parent directory

    int i=0;int j;
    for(;i<d->size;i++){
        if(strcmp(d->entries[i].name,name)==0){
            break;
            }
        }
    target_inode=d->entries[i].inode;

    if(i==d->size-1){//last entry
    d->entries[i].inode=0;//clear entry in parent directory
    strncpy(d->entries[i].name,"",64);
    d->size--;
    }
    else{
    int j;
    for(j=i;j<d->size-1;j++){

    d->entries[j].inode = d->entries[j+1].inode;
    strncpy(d->entries[j].name,d->entries[j+1].name,64);

    }
    d->size--;
    }
    rewind(fp);
    fseek(fp,512*d->block_num,SEEK_SET);
    fwrite(d,sizeof(struct dir),1,fp);//update parent dir

    rewind(fp);
    struct inode *ino = malloc(sizeof(struct inode));
    struct block *b = malloc(sizeof(struct block));
    fseek(fp,512*target_inode,SEEK_SET);
    fread(ino,sizeof(struct inode),1,fp);
    rewind(fp);
    fseek(fp,512*ino->datablocks[0],SEEK_SET);
    fread(b,sizeof(struct block),1,fp);

    if(b->block_type=='e'){//file

    for(i=0;i<ino->size;i++)wipe_block(ino->datablocks[i],'d');

    wipe_block(ino->block_num,'i');

    }
    if(b->block_type=='f'){//directory

    strncat(childpath,"/",strlen(childpath)+1);
    strncat(childpath,name,strlen(childpath)+strlen(name));

    struct dir *directory = malloc(sizeof(struct dir));
    rewind(fp);
    fseek(fp,512*ino->datablocks[0],SEEK_SET);
    fread(directory,sizeof(struct dir),1,fp);
    int k=0;

    for(;k<directory->size;k++){
    remove_entry(childpath,directory->entries[k].name);
    }
    wipe_block(b->block_num,'d');
    wipe_block(ino->block_num,'i');
    }

    fclose(fp);
    free(d);
    free(in);
    printf("Entry deleted\n");
    return 1;

}

char *gen(int n){

char *c = NULL;
c = malloc(sizeof(char)*(n+1));
int i;
int l=0;

for(i=0;i<n;i++){
l += sprintf(&c[l],"%d",i%10);
}
return c;
}

void test(){

int g = 2040;

char *c = malloc((g));
char *b = malloc(g);

c= gen(g);

makedir("","f1");
makedir("/f1/","f2");
renamedir("/f1/f2/","f3");
create("/f1/f3/","file.txt");
writefile("/f1/f3/","file.txt",c);

b =readfile("/f1/f3/","file.txt");

//printf("\n");
//printf("Original ...\n");
//printf("%s\n",c);

printf("comparing contents\n");
printf("%d",strcmp(b,c));

}

int main()
{
    printf("Size of block: %lu , inode %lu ,infoblock %lu\n",sizeof(struct block),sizeof(struct inode),sizeof(struct infoblock));
    printf("Size of directory: %lu , file %lu , indirect block %lu \n",sizeof(struct dir),sizeof(struct file),sizeof(struct indirect_block));
    printf("\n");
    format();
    //check_space(5120,0);
    //wipe();
    //printf("Next free block at %d \n",next_free_block(1));
    //printf("Next free inode at %d \n",next_free_inode(1));
    //mount();
    //makedir("","f1");
    //renamedir("/f1/f2/","f3");
    //create("","file.txt");
    //readdir("");
    //writefile("","file.txt","Mary had a little lamb");
    //appendfile("","file.txt"," with fleece as white as snow");
    //char *rd = malloc(1024);
    //rd = readfile("","file.txt");
    //printf("%s\n",rd);
    //printf("\n");
    //remove_entry("","file.txt");
    //readdir("");
    //test();
    //verify();
    char *c = malloc(1024);
    c =gen(1024);
    char *rd = malloc(1024);
    makedir("","f1");
    makedir("/f1/","f3");
    create("/f1/f3","file.txt");
    writefile("/f1/f3","file.txt",c);
    rd = readfile("f1/f3","file.txt");
    readdir("/f1/f3");
    printf("file contents\n");
    printf("%s \n",rd);
    printf("memory \n");
    printf("%s \n",c);
    verify();

    return 1;
}
