//
// Created by Administrator on 2017/2/17.
//
#include <sys/time.h>
#include "unp.h"

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