//
// Created by Administrator on 2017/3/21.
//
#include "unp.h"
#include <iostream>
using namespace std;

void echo_server(int sockfd,struct sockaddr *addr,socklen_t addrlen) {
    int n;
    socklen_t len;
    char buff[MAX_BUFF_SIZE] = {0};

    for(;;){
        len = addrlen;
        n = recvfrom(sockfd,buff,MAX_BUFF_SIZE,0,addr,&len);
        sendto(sockfd,buff,n,0,addr,len);
    }
}

int main(int argc,char **argv){
    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd < 0)
        err_sys("socket error");

    struct sockaddr_in addr,clientaddr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    inet_pton(AF_INET,"127.0.0.1",&addr.sin_addr);

    bind(sockfd,(struct sockaddr*)&addr, sizeof(addr));

    echo_server(sockfd,(struct sockaddr*)&clientaddr, sizeof(clientaddr));

    return 0;
}

