//
// Created by Administrator on 2017/1/6.
//

#ifndef UNP_UNP_H
#define UNP_UNP_H
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <assert.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <iostream>
#include <signal.h>
#include <libgen.h>
#include <cmath>
#include <unistd.h>
#include <wait.h>
#include <time.h>

#include <_timeval.h>

#define HAVA_VSNPRINTF 1
#define MAXLINE 4096
#define LISTENQ 1024
#define  MAX_BUFF_SIZE 1024

ssize_t  readn(int fd,void *ptr,size_t n);
ssize_t Readline(int fd, void *ptr, size_t maxlen);
ssize_t writen(int fd,const void *ptr,size_t n);


void Listen(int sockfd,int backlog);
void Writen(int fd, void *ptr, size_t nbytes);
void Connect(int fd, const struct sockaddr *sa, socklen_t salen);
int sock_to_family(int sockfd);


void err_ret(const char *fmt,...);
void err_sys(const char *fmt,...);
void err_dump(const char *fmt,...);
void err_msg(const char *fmt,...);
void err_quit(const char *fmt,...);

char* gf_time();
int setnonblock(int sockfd);

typedef void (*sigFunc)(int);
sigFunc Signal(int sig,sigFunc func);
#endif //UNP_UNP_H
