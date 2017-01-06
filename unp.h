//
// Created by Administrator on 2017/1/6.
//

#ifndef UNP_UNP_H
#define UNP_UNP_H
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <in6addr.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <cstring>
#include <errno.h>

ssize_t  readn(int fd,void *ptr,size_t n);
ssize_t writen(int fd,const void *ptr,size_t n);
#endif //UNP_UNP_H
