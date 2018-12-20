/*
该程序存在切尚未解决的问题
1.char *begin[CONN_MAX]数组没办法在文件发送完毕后讲所在下标的内容
删除，主要原因是无法判断文件何时发送完毕，目前暂时的处理方法是讲数组设
置的足够大，所以无法长期运行。

*/
#include "work.h"
#include "tpool.h"

int main(int argc, char **argv)
{
    printf("##################### Server #####################\n");
    int port = PORT;
    if (argc>1)
        port = atoi(argv[1]);

    /*创建线程池*/
    if (tpool_create(THREAD_NUM) != 0) {
        printf("tpool_create failed\n");
        exit(-1);
    }
    printf("--- Thread Pool Strat ---\n");

    /*初始化server，监听请求*/
    int listenfd = Server_init(port);
    socklen_t sockaddr_len = sizeof(struct sockaddr);

    /*epoll*/
    static struct epoll_event ev, events[EPOLL_SIZE];
	int epfd = epoll_create(EPOLL_SIZE);
	ev.events = EPOLLIN;
	ev.data.fd = listenfd;
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);        //水平触发，有待商榷

    while(1){
        int events_count = epoll_wait(epfd, events, EPOLL_SIZE, -1);
        int i=0;

        /*接受连接，添加work到work-Queue*/
        for(; i<events_count; i++){
            if(events[i].data.fd == listenfd)
            {
                int connfd;
                struct sockaddr_in  clientaddr;
                while( ( connfd = accept(listenfd, (struct sockaddr *)&clientaddr, &sockaddr_len) ) > 0 )
				{
				    printf("EPOLL: Received New Connection Request---connfd= %d\n",connfd);
					struct args *p_args = (struct args *)malloc(sizeof(struct args));
                    p_args->fd = connfd;
                    p_args->recv_finfo = recv_fileinfo;
                    p_args->recv_fdata = recv_filedata;

                    /*添加work到work-Queue*/
                    tpool_add_work(worker, (void*)p_args);
				}
            }
        }
    }

    return 0;
}
