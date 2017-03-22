#include "unp.h"
#include <iostream>


int main(int argc,char **argv){
    if(argc != 2){
        err_quit("usage : program_name ip_address");
    }

    int sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(sockfd < 0 )
        err_sys("socket error");

    struct  sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(13);
    if(inet_pton(AF_INET,argv[1],&addr.sin_addr) <= 0){
        err_quit("inet_pton");
    }
    socklen_t  addrLen = sizeof(addr);

    int rcvBufSize = 0;
    socklen_t bufLen = sizeof(rcvBufSize);

    if(getsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&rcvBufSize, &bufLen) < 0){
        err_sys("getsockopt rcvbuf size error");
    }

    int mss = 0;
    socklen_t  mssLen = sizeof(mss);
    if(getsockopt(sockfd,IPPROTO_TCP,TCP_MAXSEG,&mss, &mssLen) < 0){
        err_sys("getsockopt mss error");
    }
    std::cout <<  "rcvbuf size = " << rcvBufSize << ",mss = " << mss << std::endl;

    if(connect(sockfd,(struct sockaddr*)&addr,addrLen) < 0)
        err_sys("connect error");

    if(getsockopt(sockfd,SOL_SOCKET,SO_RCVBUF,&rcvBufSize, &bufLen) < 0){
        err_sys("getsockopt rcvbuf size error");
    }

    if(getsockopt(sockfd,IPPROTO_TCP,TCP_MAXSEG,&mss, &mssLen) < 0){
        err_sys("getsockopt mss error");
    }
    std::cout <<  "rcvbuf size = " << rcvBufSize << ",mss = " << mss << std::endl;

    close(sockfd);
    return 0;
}

