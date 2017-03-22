#include "unp.h"
#include <iostream>

int main(){
    int tcp_sockfd =socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    if(tcp_sockfd < 0)
        err_sys("tcp socket error");
    int udp_sockfd = socket(AF_INET,SOCK_DGRAM,IPPROTO_UDP);
    if(udp_sockfd < 0)
        err_sys("udp socket error");

    int tcp_rcvbuf_size = 0;
    socklen_t  len = sizeof(tcp_rcvbuf_size);
    if(getsockopt(tcp_sockfd,SOL_SOCKET,SO_RCVBUF,&tcp_rcvbuf_size, &len) < 0)
        err_sys("getsockopt tcp rcvbuf error");

    int tcp_sndbuf_size = 0;
    len = sizeof(tcp_sndbuf_size);
    if(getsockopt(tcp_sockfd,SOL_SOCKET,SO_SNDBUF,&tcp_sndbuf_size, &len) < 0)
        err_sys("getsockopt tcp sndbuf error");

    int udp_rcvbuf_size = 0;
    len = sizeof(udp_rcvbuf_size);
    if(getsockopt(udp_sockfd,SOL_SOCKET,SO_RCVBUF,&udp_rcvbuf_size, &len) < 0)
        err_sys("getsockopt tcp rcvbuf error");

    int udp_sndbuf_size = 0;
    len = sizeof(udp_sndbuf_size);
    if(getsockopt(udp_sockfd,SOL_SOCKET,SO_SNDBUF,&udp_sndbuf_size, &len) < 0)
        err_sys("getsockopt tcp sndbuf error");

    std::cout << "tcp_rcvbuf_size = " << tcp_rcvbuf_size << std::endl;
    std::cout << "tcp_sndbuf_size = " << tcp_sndbuf_size << std::endl;
    std::cout << "udp_rcvbuf_size = " << udp_rcvbuf_size << std::endl;
    std::cout << "udp_sndbuf_size = " << udp_sndbuf_size << std::endl;

    close(tcp_sockfd);
    close(udp_sockfd);
}

