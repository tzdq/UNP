//
// Created by Administrator on 2017/2/17.
//
#include <sys/time.h>
#include <w32api/ws2tcpip.h>
#include "unp.h"

#define ERROR_PROCESS(sockfd) \
    close(sockfd);\
    return -1;

void Listen(int sockfd, int backlog) {
    char *ptr;
    if((ptr = getenv("LISTENQ")) != NULL)
        backlog = atoi(ptr);
    if(listen(sockfd,backlog) < 0)
        err_sys("listen error");
}

int sock_to_family(int sockfd) {
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    if(getsockname(sockfd,(struct sockaddr*)&addr,&addrlen) < 0)
        return  -1;
    return (addr.sin_family);
}

void Connect(int fd, const struct sockaddr *sa, socklen_t salen)
{
    if (connect(fd, sa, salen) < 0)
        err_sys("connect error");
}

char* gf_time(){
    struct  timeval tv;
    static char str[30] = {0};
    char *ptr;
    if(gettimeofday(&tv,NULL) < 0)
        err_sys("gettimeofday error");

    ptr = ctime(&tv.tv_sec);
    bzero(str,sizeof(str));
    strcpy(str,&ptr[11]);
    snprintf(str+8,sizeof(str) - 8,".%06ld",tv.tv_usec);

    return  str;
}

int setnonblock(int sockfd){
    int old_fcntl_value = fcntl(sockfd,F_GETFL);
    fcntl(sockfd,F_SETFL,old_fcntl_value|O_NONBLOCK);
    return old_fcntl_value;
}

//非阻塞connect期望的错误是EINPROGRCESS
//0 表示成功 -1表示 失败
int connect_nonblock_select(int sockfd,const struct sockaddr* addr,socklen_t addrlen,int nsec){
    int flags,ret;

    //可以直接使用上面的setnonblock
    flags = fcntl(sockfd,F_GETFL);
    fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);

    if((ret = connect(sockfd,addr,addrlen)) < 0){
        printf("connect error errno = %d\n",errno);
        if(errno != EINPROGRESS)
            return -1;
    }
    //立即连上，在同一台服务器
    if(ret == 0){
        printf("connect server immediately\n");
        fcntl(sockfd,F_SETFL,flags);
        return 0;
    }

    fd_set wset;
    FD_ZERO(&wset);
    FD_SET(sockfd,&wset);

    //超时时间
    struct timeval tval;
    tval.tv_sec = nsec;
    tval.tv_usec = 0;

    ret = select(sockfd+1,NULL,&wset,NULL,nsec ? &tval : NULL);
    if(ret <= 0){//超时或出错
        printf("timeout or error\n");
        errno = ETIMEDOUT;
        return -1;
    }
    //连接失败  不可写
    if(! FD_ISSET(sockfd,&wset)){
        printf("socket not set\n");
        return -1;
    }
    //获取并清除socket上的错误
    int error = 0;
    socklen_t  len = sizeof(error);
    if(getsockopt(sockfd,SOL_SOCKET,SO_ERROR,&error,&len) < 0){
        printf("getsockopt error\n");
        return -1;
    }
    //错误号不为0，表示有错误发生
    if(error != 0){
        printf("after select error occues\n");
        errno =  error;
        return -1;
    }
    //链接成功
    fcntl(sockfd,F_SETFL,flags);
    return 0;
}

//非阻塞connect期望的错误是EINPROGRCESS
//0 表示成功 -1表示 失败
int connect_nonblock_epoll(int sockfd,const struct sockaddr* addr,socklen_t addrlen,int nsec){
#ifndef DONT_SUPPORT_EPOLL
    int flags,ret;
    //可以直接使用上面的setnonblock
    flags = fcntl(sockfd,F_GETFL);
    fcntl(sockfd,F_SETFL,flags|O_NONBLOCK);

    if((ret = connect(sockfd,addr,addrlen)) < 0){
        //printf("connect error errno = %d,inprogress=%d\n",errno,EINPROGRESS);
        if(errno != EINPROGRESS)
            return -1;
    }
    //立即连上，在同一台服务器
    if(ret == 0){
        printf("connect server immediately\n");
        fcntl(sockfd,F_SETFL,flags);
        return 0;
    }

    epoll_event events[MAX_EPOLL_EVENT_SIZE];
    epoll_event event;
    event.data.fd = sockfd;
    event.events = EPOLLOUT|EPOLLET;

    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    epoll_ctl(epollfd,EPOLL_CTL_ADD,sockfd,&event);

    ret = epoll_wait(epollfd,events,MAX_EPOLL_EVENT_SIZE,nsec );
    if(ret == 0){//超时或出错
        printf("timeout or error\n");
        errno = ETIMEDOUT;
        return -1;
    }
    //连接失败  不可写
    //只有一个其实可以不用写for循环
    if(!events[0].events & EPOLLOUT){
        ERROR_PROCESS(epollfd);
    }
    //链接成功
    fcntl(sockfd,F_SETFL,flags);
    close(epollfd);
#else
#endif
    return 0;
}

int tcp_connect(const char *host,const char *serv){
    int sockfd,n;

    struct addrinfo hints,*res,*ressave;
    bzero(&hints,sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((n  = getaddrinfo(host,serv,&hints,&res)) != 0){
        err_quit("tcp_connect error for %s, %s: %s",host,serv,gai_strerror(n));
    }
    ressave =res;

    do{
        sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
        if(sockfd < 0)continue;

        if(connect(sockfd,res->ai_addr,res->ai_addrlen) == 0)break;
        close(sockfd);
    }while((res = res->ai_next) != NULL);

    if(res == NULL)
        err_sys("tcp_connect error for %s,%s",host,serv);

    freeaddrinfo(ressave);
    return sockfd;
}

struct addrinfo* Host_serv(const char* host,const char* serv,int family,int socktype){
    int n;
    struct addrinfo hints,*res;
    bzero(&hints,sizeof(hints));
    hints.ai_family = family;
    hints.ai_flags = AI_CANONNAME;
    hints.ai_socktype = socktype;

    if((n = getaddrinfo(host,serv,&hints,&res)) != 0)
        err_quit("host_serv error for %s,%s:%s"
                ,(host == NULL ? ("no hostname") : host)
                ,(serv == NULL ? ("no service name") : serv)
                ,gai_strerror(n));
    return res;
}

int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
       struct timeval *timeout)
{
    int     n;
    if ( (n = select(nfds, readfds, writefds, exceptfds, timeout)) < 0)
        err_sys("select error");
    return(n);      /* can return 0 on timeout */
}


void *Malloc(size_t size)
{
    void    *ptr;

    if ( (ptr = malloc(size)) == NULL)
        err_sys("malloc error");
    return(ptr);
}


struct addrinfo* host_serv(const  char *hostname,const char *service,int family,int socktype){
    int ret = 0;
    struct addrinfo hint,*res;
    bzero(&hint,sizeof(addrinfo));
    hint.ai_family = family;
    hint.ai_socktype = socktype;
    hint.ai_flags = AI_CANONNAME;

    if((ret = getaddrinfo(hostname,service,&hint,&res)) != 0)
        return NULL;
    return res;
}

int tcp_listen(const char *hostname,const char *service,socklen_t *len){
    int ret = 0;
    int sockfd;
    const  int on = 1;

    struct  addrinfo *res,hint,*tmp;
    bzero(&hint,sizeof(addrinfo));
    hint.ai_flags = AI_PASSIVE;
    hint.ai_socktype = SOCK_STREAM;
    hint.ai_family = AF_UNSPEC;

    if((ret = getaddrinfo(hostname,service,&hint,&res)) != 0)
        err_quit("getaddrinfo error,hostname = %s,service = %s,strerr = %s",hostname,service,gai_strerror(ret));
    tmp = res;

    do{
        sockfd = socket(res->ai_family,res->ai_socktype,res->ai_protocol);
        if(sockfd < 0) continue;

        setsockopt(sockfd,SOL_SOCKET,SO_REUSEADDR,&on, sizeof(on));

        if(bind(sockfd,res->ai_addr,res->ai_addrlen) == 0)
            break;

        close(sockfd);
    }while(res->ai_next != NULL);
    if(res == NULL)
        err_sys("tcp listen error for %s,%s",hostname,service);
    Listen(sockfd,LISTENQ);

    if(len)
        *len = res->ai_addrlen;

    freeaddrinfo(tmp);
    return sockfd;
}