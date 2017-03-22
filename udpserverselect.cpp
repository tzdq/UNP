//
// Created by Administrator on 2017/3/22.
//
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

void str_echo(int sockfd){
    ssize_t  n;
    char buf[MAX_BUFF_SIZE] = {0};
    again:
    while((n = read(sockfd,buf,MAX_BUFF_SIZE)) > 0){
        Writen(sockfd,buf,n);
    }
    if(n < 0 && errno == EINTR)
        goto again;
    else if(n < 0)
        err_sys("str_echo:read echo");
}

int main(int argc,char **argv){
    int tcp_sock_fd = socket(AF_INET,SOCK_STREAM,0);
    if(tcp_sock_fd == -1){
        err_sys("tcp socket error");
    }
    struct sockaddr_in serv_addr,client_addr;
    bzero(&serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8888);
    inet_pton(AF_INET,"127.0.0.1",&serv_addr.sin_addr);

    socklen_t len = 0;
    char buf[MAX_BUFF_SIZE] = {0};
    int conn_fd;

    const int on = 1;
    setsockopt(tcp_sock_fd,SOL_SOCKET,SO_REUSEADDR,&on, sizeof(on));

    int ret = bind(tcp_sock_fd,(struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if(ret < 0){
        close(tcp_sock_fd);
        err_sys("tcp bind error");
    }

    ret = listen(tcp_sock_fd,5);
    if(ret < 0){
        close(tcp_sock_fd);
        err_sys("tcp listen error");
    }

    int udp_sock_fd = socket(AF_INET,SOCK_DGRAM,0);
    if(udp_sock_fd < 0){
        err_sys("socket error");
    }

    struct sockaddr_in udp_serv_addr;
    bzero(&udp_serv_addr, sizeof(udp_serv_addr));
    udp_serv_addr.sin_family = AF_INET;
    udp_serv_addr.sin_port = htons(8888);
    inet_pton(AF_INET,"127.0.0.1",&udp_serv_addr.sin_addr);

    bind(udp_sock_fd,(struct sockaddr*)&udp_serv_addr, sizeof(udp_serv_addr));

    Signal(SIGCHLD,sig_chld);
    fd_set rset;
    FD_ZERO(&rset);
    int maxfd = max(udp_sock_fd,tcp_sock_fd) + 1;

    for(;;){
        FD_SET(tcp_sock_fd,&rset);
        FD_SET(udp_sock_fd,&rset);

        int nReady = select(maxfd,&rset,NULL,NULL,NULL);
        if(nReady < 0){
            if(errno == EINTR)continue;
            else
                err_sys("select error");
        }
        if(FD_ISSET(tcp_sock_fd,&rset)){
            len = sizeof(client_addr);
            conn_fd = accept(tcp_sock_fd,(struct sockaddr*)&client_addr,&len);
            if((ret = fork()) == 0){
                close(tcp_sock_fd);
                str_echo(conn_fd);
                exit(0);
            }
            close(conn_fd);
        }

        if(FD_ISSET(udp_sock_fd,&rset)){
            len = sizeof(client_addr);
            ret = recvfrom(udp_sock_fd,buf,MAX_BUFF_SIZE,0,(struct sockaddr*)&client_addr,&len);
            sendto(udp_sock_fd,buf,ret,0,(struct sockaddr*)&client_addr,len);
        }
    }
    return 0;
}

