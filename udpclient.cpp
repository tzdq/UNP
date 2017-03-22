//
// Created by Administrator on 2017/3/21.
//

//
// Created by Administrator on 2017/3/21.
//
#include "unp.h"
#include <iostream>
using namespace std;

void echo_client(FILE* fp,int sockfd,struct sockaddr *addr,socklen_t addrlen) {
    int n;
    socklen_t len;
    connect(sockfd,addr,addrlen);
    char  recvbuff[MAX_BUFF_SIZE] = {0},sendbuff[MAX_BUFF_SIZE] = {0};
    while (fgets(sendbuff,MAX_BUFF_SIZE,fp) != NULL){
        write(sockfd,sendbuff,strlen(sendbuff));
        n = read(sockfd,recvbuff,MAX_BUFF_SIZE);
        recvbuff[n] = 0;
        fputs(recvbuff,stdout);
    }
}

int main(int argc,char **argv){
    int sockfd = socket(AF_INET,SOCK_DGRAM,0);
    if(sockfd < 0)
        err_sys("socket error");

    struct sockaddr_in servaddr;
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
#if 1
    servaddr.sin_port = htons(8888);
#else
    servaddr.sin_port = htons(7);
#endif

    inet_pton(AF_INET,"127.0.0.1",&servaddr.sin_addr);

    echo_client(stdin,sockfd,(struct sockaddr*)&servaddr, sizeof(servaddr));

    return 0;
}

