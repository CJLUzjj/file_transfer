#include "works.h"

int freeid = 0;
struct conn gconn[CONN_MAX];                            //存放连接数据，需加锁
pthread_mutex_t conn_lock = PTHREAD_MUTEX_INITIALIZER;  //初始化锁

int svfile_num = 0;
struct save_file svfile[SAVE_NUM];                      //存放服务器数据，需加锁
pthread_mutex_t file_lock = PTHREAD_MUTEX_INITIALIZER;  //初始化锁

//定义所有结构体长度
int fileinfo_len = sizeof(struct fileinfo);             //文件信息长度
socklen_t sockaddr_len = sizeof(struct sockaddr);       //地址长度
int head_len = sizeof(struct head);                     //分块头部信息长度
int conn_len = sizeof(struct conn);                     //与客户端关联连接的长度
int save_file_len = sizeof(struct save_file);           //与保存服务器文件关联信息的长度

//记录map地址数组，因为有可能多个客户端请求发送数据
extern char *mbegin[CONN_MAX];

//创建管道
int pipe( int pipefd[2] );
//自旋锁，用于发送线程争夺文件描述符
//pthread_mutex_t fd_lock = PTHREAD_MUTEX_INITIALIZER;

//文件创建
int creatfile(char *filename, int size){
    int fd = open(filename, O_RDWR | O_CREAT);
    fchmod(fd, S_IRUSR|S_IWUSR|S_IRGRP|S_IROTH);
    lseek(fd, size-1, SEEK_SET);
    write(fd, "", 1);                                   //写入结束符？
    close(fd);
    return 0;
}

//工作线程，分析状态机
void * worker(void *argc){
    struct args *pw = (struct args *)argc;
    int  conn_fd = pw->fd;

    char type_buf[INT_SIZE] = {0};
    char *p = type_buf;
    if(INT_SIZE != recv(conn_fd, p, INT_SIZE, 0)){
        printf("recv type failed!\n");
        exit(-1);
    }

    int type = *((int*)type_buf);
    printf("test:type is %d\n", type);

    switch (type){
        //接受文件信息
        case 0:
            printf("## worker ##\nCase %d: the work is recv file_info\n", type);
            pw->recv_finfo(conn_fd);
            break;
        //接受文件块
        case 1:
            printf("## worker ##\nCase %d: the work is recv file_data\n", type);
            pw->recv_fdata(conn_fd);
            break;
        //发送所有文件信息
        case 2:
            printf("## worker ##\nCase %d: the work is send all file_info\n", type);
            pw->send_finfo(conn_fd);
            break;
        //发送文件块
        case 3:
            printf("## worker ##\nCase %d: the work is send file_data\n", type);
            pw->send_fdata(conn_fd);
            break;
        //向管道写入fd来与线程连接，因为线程无法主动与客户端连接所有只能客户端创建连接来与线程连接
        case 4:
            printf("## worker ##\nCase %d: the work is write in pipe\n", type);
            char connfdbuf[INT_SIZE] = {0};
            memcpy(connfdbuf, &conn_fd, INT_SIZE);
            send(pipefd[1], connfdbuf, INT_SIZE, 0);
            break;
        default:
            printf("unknown type!");
            return NULL;
    }

    return NULL;
}

//专门由于处理发送数据的发送线程
void * sender(void *args){
    //发送数据块头部
    struct head * p_fhead = (struct head *)args;
    printf("------- blockhead -------\n");
    printf("filename= %s\nThe filedata id= %d\noffset= %d\nbs= %d\n", p_fhead->filename, p_fhead->id, p_fhead->offset, p_fhead->bs);
    printf("-------------------------\n");
    char sdbuf[INT_SIZE] = {0};
    recv(pipefd[0], sdbuf, INT_SIZE, 0);
    fd = *((int *)sdbuf);
    char send_buf[100] = {0};
    memcpy(send_buf, p_fhead, head_len);
    char *p = send_buf;
    if(head_len != send(fd, p, head_len, 0)){
        printf("send headinfo failed!\n");
    }
    //发送数据块
    printf("Thread : send filedata\n");
    int send_size = 0, num = p_fhead->bs/SEND_SIZE;
    char *fp = mbegin+p_head->offset;
    for(int i=0;i <num; i++){
        if((send_size = send(sock_fd, fp, SEND_SIZE, 0)) == SEND_SIZE){
            fp+=SEND_SIZE;
            printf("fp = %p ; a SEND_SIZE of\n", fp);
        }
        eles{
            printf("send_size = %d ; a SEND_SIZE erre\n", sendsize);
        }
    }

    printf("##### send a fileblock ####\n");
    close(fd);
    free(args);
    return NULL;
}

//接受文件信息，添加连接到gconn数组，创建文件，map到内存中
void recv_fileinfo(int sockfd)
{
    char fileinfo_buf[100] = {0};
    bzero(fileinfo_buf, fileinfo_len);
    if(fileinfo_len != recv(sockfd, &fileinfo_buf, fileinfo_len, 0)){
        printf("recv fileinfo failed!\n");
        exit(-1);
    }
    
    struct fileinfo finfo;
    memcpy(&finfo, fileinfo_buf, fileinfo_len);

    //文件信息
    printf("------- fileinfo -------\n");
    printf("filename = %s\nfilesize = %d\ncount = %d\nbs = %d\n", finfo.filename, finfo.filesize, finfo.count, finfo.bs);
    printf("------------------------\n");

    pthread_mutex_lock(&file_lock);
    printf("test:file_lock is lock\n");
    //判断可存放文件数量是否已满
    if(svfile_num == save_file_len){
        printf("server file full!\n");
        send(sockfd, "255", INT_SIZE);
        printf("test:file_lock is unlock\n");
        pthread_mutex_unlock(&file_lock);
        return;
    }
    //没满的话就存入文件信息到svfile
    for(int i=0;i<save_file_len;i++){
        if(svfile[i].used == 1 || svfile[i].used == 2)
            continue;
        svfile[i].filename = finfo.filename;
        svfile[i].filesize = finfo.filesize;
        svfile[i].id = i;
        svfile[i].used = 1;
        printf("test:file is save,id is %d,filename is %s,filesize is %d\n", i, svfile[i].filename, svfile[i].filesize);
        svfile_num++;
        break;
    }
    printf("test:file_lock is unlock\n");    
    pthread_mutex_unlock(&file_lock);

    //创建文件，map到虚存
    char filepath[100] = {0};
    strcpy(filepath, finfo.filename);
    createfile(filepath, finfo.filesize);
    int fd = 0;
    if((fd = open(filepath, O_RDWR)) == -1 ){
        printf("open file error\n");
        exit(-1);
    }
    printf("test: fd = %d\n", fd);

    //创建文件映射
    char *map = (char *)mmap(NULL, finfo.filesize, PROT_WRITE|PROT_READ, MAP_SHARED, fd, 0);
    printf("test: map = %p", map);
    close(fd);

    //向gconn添加连接
    pthread_mutex_lock(&conn_lock);

    printf("recv_fileinfo(): Lock conn_lock, wenter gconn[]\n");
    //寻找一个可用的id
    while( gconn[freeid].used ){
        ++freeid;
        freeid = freeid%CONN_MAX;
        sleep(1);               //防止无可用id while空循环浪费cpu
    }
    printf("test:freeid is %d", freeid);

    bzero(&gconn[freeid].filename, FILENAME_MAXLEN);
    gconn[freeid].info_fd = sockfd;
    strcpy(gconn[freeid].filename, finfo.filename);
    gconn[freeid].filesize = finfo.filesize;
    gconn[freeid].num = finfo.num;
    gconn[freeid].bs = finfo.bs;
    gconn[freeid].count = 0;
    gconn[freeid].used = 1;

    pthread_mutex_unlock(&conn_lock);

    printf("recv_fileinfo(): unlock conn_lock, exit gconn[]\n");

    //想send client发送分配的freeid,作为确认，每个分块都将携带id
    char freeid_buf[INT_SIZE] = {0};
    memcpy(freeid_buf, &freeid, INT_SIZE);
    send(sockfd, freeid_buf, INT_SIZE, 0);
    printf("freeid = %d\n", *(int *)freeif_buf);

    return;
}

//接受文件块
void recv_filedata(int sockfd)
{
    //读取分块头部信息
    int recv_size = 0;
    char head_buf[100] = {0};
    char *p = head_buf;
    if(head_len != recv(sockfd, p, head_len, 0)){
        printf("recv head_info failed!\n");
        exit(-1);
    }

    struct head fhead;
    memcpy(&fhead, head_buf, head_len);
    int recv_id = fhead.id;

    //计算该块在map中的起始地址fp*
    int recv_offset = fhead.offset;
    char *fp = gconn[recv_id].mbegin+recv_offset;

    printf("------- blockhead -------\n");
    printf("filename = %s\nThe filedata id = %d\noffset=%d\nbs = %d\nstart addr= %p\n", fhead.filename, fhead.id, fhead.offset, fhead.bs, fp);
    printf("-------------------------\n");

    //接收数据，往map内存写
    int remain_size = fhead.bs;         //数据快中待接收数据大小
    int size = 0;                       //一次recv接受数据大小
    while(remain_size > 0){
        if((size = recv(sockfd, fp, RECVBUF_SIZE, 0)) >0){
            fp+=size;
            remain_size-=size;
            printf("test:recv size = %d        ", size);
            printf("remain size = %d\n", remain_size);
        }
    }

    printf("-------------recv a fileblock------------\n");

    //修改gconn,判断是否时最后一个分块
    pthread_mutex_lock(&conn_lock);
    gconn[recv_id].count++;
    if(gconn[recv_id].count == gconn[recv_id].num){
        munmap((void *)gconn[recv_id].mbegin, gconn[recv_id].filesize);

        printf("-----------------recv a file--------------------\n");


        int fd = gconn[recv_id].info_fd;
        close(fd);
        pthread_mutex_lock(&file_lock);
        printf("test:file_lock is lock\n");
        for(int i=0;i<svfile_num;i++){
            if(gconn[recv_id].filename == svfile[i].name){
                svfile[i].used == 2;
                printf("test:svfile[%d].used change 2\n", i);
                break;
            }
        }
        printf("test:file_lock is unlock\n");
        pthread_mutex_unlock(&file_lock);
        bzero(&gconn[recv_id], conn_len);

    }
    pthread_mutex_unlock(&conn_lock);

    close(sockfd);
    return;
}

//发送所有文件信息
void send_fileinfo(int sockfd){
    char send_buf[save_file_len*SAVE_NUM];
    struct save_file *pfile = NULL;
    int count_mem = 0;
    pthread_mutex_lock(&file_lock);
    printf("test:filelock is lock\n");
    for(int i=0;i<svfile_num;i++){
        if(svfile[i].used == 2){
            pfile->filename = svfile[i].filename;
            pfile->filesize = svfile[i].filesize; 
            pfile->used = svfile[i].used;
            memcpy(send_buf+count_mem*save_file_len, pfile, save_file_len);
            count_mem++; 
            printf("test:filename=%s,filesize=%d,used=%d\n", pfile->filename,pfile->filesize,pfile->used);
        }
    }
    printf("file_lock is unlock\n");
    pthread_mutex_unlock(&file_lock);
    send(sockfd, send_buf, save_file_len*count_mem, 0);

    printf("---------fileinfo------------");
    return;
}

//发送具体文件信息
void send_fileinfo1(int sock_fd, char *fname, struct stat* p_fstat, struct fileinfo *p_finfo, int *p_last_bs)
{	
	/*初始化fileinfo*/
    bzero(p_finfo, fileinfo_len);
    strcpy(p_finfo->filename, fname);
    p_finfo->filesize = p_fstat->st_size;

    /*最后一个分块可能不足一个标准分块*/
    int count = p_fstat->st_size/BLOCKSIZE;
    if(p_fstat->st_size%BLOCKSIZE == 0){
        p_finfo->count = count;
    }
    else{
        p_finfo->count = count+1;
        *p_last_bs = p_fstat->st_size - BLOCKSIZE*count;
    }
    p_finfo->bs = BLOCKSIZE;

    /*发送type和文件信息*/
    char send_buf[100]= {0};
    int type=0;
    memcpy(send_buf, &type, INT_SIZE);
    memcpy(send_buf+INT_SIZE, p_finfo, fileinfo_len);
    send(sock_fd, send_buf, fileinfo_len+INT_SIZE, 0);

	printf("-------- fileinfo -------\n");
    printf("filename= %s\nfilesize= %d\ncount= %d\nblocksize= %d\n", p_finfo->filename, p_finfo->filesize, p_finfo->count, p_finfo->bs);
	printf("-------------------------\n");
    return;
}

//生成头部信息
struct head * new_fb_head(char *filename, int freeid, int *offset)
{
    struct head * p_fhead = (struct head *)malloc(head_len);
    bzero(p_fhead, head_len);
    strcpy(p_fhead->filename, filename);
    //废物利用，在客户端中id用于记录mbegin数组下标来确定具体文件的具体下标
    p_fhead->id = freeid;               
    p_fhead->offset = *offset;
    p_fhead->bs = BLOCKSIZE;
    *offset += BLOCKSIZE;
    return p_fhead;
}

//发送文件
void send_filedata(int sockfd){
    char id_buf[INT_SIZE] = {0};
    recv(sockfd, id_buf, INT_SIZE, 0);
    int id = *((int *)id_buf);
    printf("test:id is %d", id);
    char openfile = svfile[id].filename;
    int opfd = 0;
    if(opfd = open(openfile, O_RDWR) == -1){
        printf("open erro!\n");
        exit(-1);
    }
    struct stat opfilestat;
    fstat(opfd, &opfilestat);
    int last_bs = 0;
    struct fileinfo sdfinfo;
    send_fileinfo1(opfd, openfile, &opfilestat, &sdfinfo, &last_bs);
    int blnum = -1;              //用于判断是否有空间map,并用于记录数组下标
    for(int i=0;i<CONN_MAX;i++){
        if(mbegin[i] != NULL){
            //map,关闭fd
            mbegin[i] = (char *)mmap(NULL, opfilestat.st_size, PROT_WRITE|PROT_READ, MAP_SHARED, opfd, 0); 
            blnum = i;
            break;          
        }
    }
    if(blnum == -1){
        printf("mmap failed, no enough space!\n");
        exit(-1);
    }
    close(opfd);
    int j = 0, num = sdfinfo.count, offset=0;
    if(last_bs == 0){
        for(j=0; j<num; j++){
            struct head * p_fread = new_fb_head(openfile, blnum, &offset);
            tpool_add_work(sender, (void*)p_fread);
        }
    }
    //文件不能被标准分块
    else{
        for(j=0; j<num; j++){
            struct head * p_fhead = new_fb_head(openfile, blnum, &offset);
            tpool_add_work(sender, (void*)p_fhead);
        }
        //最后一个分块
        struct head * p_fhead = (struct head *)malloc(head_len);
        bzero(p_fhead, head_len);
        strcpy(p_fhead->filename, openfile);
        p_fhead->id = blnum;
        p_fhead->offset = offset;
        p_fhead->bs = last_bs;
        tpool_add_work(sender, (void*)p_fhead);     //将发送数据任务送进任务队列
    }
}

/*初始化Server，监听Client*/
int server_init(int port)
{
    int listen_fd;
    struct sockaddr_in server_addr;
    if((listen_fd = socket(AF_INET, SOCK_STREAM, 0))== -1)
    {
        fprintf(stderr, "Creating server socket failed.");
        exit(-1);
    }
    set_fd_noblock(listen_fd);

    bzero(&server_addr, sockaddr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    if(bind(listen_fd, (struct sockaddr *) &server_addr, sockaddr_len) == -1)
    {
        fprintf(stderr, "Server bind failed.");
        exit(-1);
    }

    if(listen(listen_fd, LISTEN_QUEUE_LEN) == -1)
    {
        fprintf(stderr, "Server listen failed.");
        exit(-1);
    }
    return listen_fd;
}

void set_fd_noblock(int fd)
{
    int flag=fcntl(fd, F_GETFL, 0);
	fcntl(fd, F_SETFL, flag | O_NONBLOCK);
	return;
}
