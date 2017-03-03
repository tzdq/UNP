//
// Created by Administrator on 2017/1/4.
//

#ifndef UNP_UTILS_H
#define UNP_UTILS_H

#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <in6addr.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <cerrno>

char* sock_ntop(const struct sockaddr* addr,socklen_t len);
int inet_pton_loose(int family,const char* strptr,void *addrptr);
int judgeEnduan();//
int my_inet_pton_ipv4(int family,const char *strptr, void *addrptr);
const char * my_inet_ntop_ipv4(int family,const void *addrptr,char *strptr,size_t addrlen);
#endif //UNP_UTILS_H
