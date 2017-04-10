//
// Created by Administrator on 2017/4/10.
//
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include "processpool.h"
#ifdef IS_USE_EPOLL
#include <sys/epoll.h>
#endif

#include <iostream>
#include <libgen.h>

int main(int argc,char**argv){
    if(argc <= 2){
        cout << "usage :"<<basename(argv[0]) << "ip_address port"<<endl;
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);
    char buf[256] = {0};

    int servfd = socket(AF_INET,SOCK_STREAM,0);
    assert(servfd >= 0);

    int ret = 0;
    struct sockaddr_in addr;
    memset(&addr,0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&addr.sin_addr);

    ret = connect(servfd,(struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    send(servfd,"hello",strlen("hello"),0);
    recv(servfd,buf,sizeof(buf),0);
    cout <<"buf = "<<buf<<endl;
    close(servfd);
    return 0;
}
