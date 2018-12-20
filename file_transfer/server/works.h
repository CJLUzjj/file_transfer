#ifndef SERVER_H__
#define SERVER_H__

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/epoll.h>
#include <sys/mman.h>

#define PORT 10001                  //listen port
#define LISTEN_QUEUE_LEN 100        //listen队列长度
#define THREAD_NUM 8                //线程池大小
#define CONN_MAX 10                 //支持最大连接数，一个连接包含多个socket连接（多线程）
#define EPOLL_SIZE 50               //epool最大监听fd数量
#define FILENAME_MAXLEN 30          //文件名最大长度
#define INT_SIZE 4                  //int类型长度
#define SAVE_NUM 30                 //服务器保存的最多文件数
#define BLOCKSIZE 536870912         //512M

//一次recv接受数据大小
#define RECVBUF_SIZE 65536          //64K

//文件信息
struct fileinfo
{
    char filename[FILENAME_MAXLEN];     //filename
    int filesize;                       //filesize
    int num;                          //分块数量
    int bs;                             //分块大小
};

//分块头部信息
struct head
{
    char filename[FILENAME_MAXLEN];     //所属文件文件名
    int id;                             //分块所属文件id， gconn数组下标
    int offset;                         //分块在文件中的偏移量
    int bs;                             //分块大小
};

//与客户端关联的连接，每次传输建立一个，在多线程之间共享
struct conn{
    int info_fd;                        //信息交换socket:接受文件信息 文件传送通知client
    char filename[FILENAME_MAXLEN];     //文件名
    char filesize;                      //file size
    int bs;                             //分块大小
    int num;                            //分块数量
    int count;                          //已接受分块的数量
    int *mbegin;                        //mmap起始地址
    int used;                           //使用标记，1表示使用中，0表示可用
};

//与保存在服务器中的文件关联的结构体，在接受客户端请求时发送
struct save_file{
    char filename[FILENAME_MAXLEN];
    int filesize;
    int id;                             //客户端通过发送id来判断需要哪个文件
    int used;                           //使用标记，1表示使用中，0表示可用,2表示可发送
};

//线程参数
struct args{
    int fd;
    void (*recv_finfo)(int fd);
    void (*recv_fdata)(int fd);
    void (*send_finfo)(int fd);
    void (*send_fdata)(int fd);
};

//创建文件
int creatfile(char *filename, int size);

//初始化server：监听请求，返回listenfd
int server_init(int port);

//设置fd非阻塞
void set_fi_noblock(int fd);

//接受文件信息，向Send Client返回本次文件传输使用的freeid
void recv_fileinfo(int sockfd);

//接受文件块
void recv_filedata(int sockfd);

//接受请求，向Recv Client返回所有文件信息和非freeid
void send_fileinfo(int sockfd);

//向recv client发送具体文件的信息
void send_fileinfo1(int sock_fd, char *fname, struct stat* P_fstat, struct fileinfo *p_finfo, int *p_lats_bs)

//生成头部信息
struct head * new_fb_head(char *filename, int freeid, int *offset)

//接受id，向Recv Client发送对应id的文件
void send_filedata(int sockfd);

//线程函数
void * worker(void *argc);

//发送线程函数
void * sender(void *argc);

#endif


