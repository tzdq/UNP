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
#include <sys/epoll.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define HAVA_VSNPRINTF 1
#define MAXLINE 4096
#define LISTENQ 1024
#define MAX_EPOLL_EVENT_SIZE 1
#define  MAX_BUFF_SIZE 1024

ssize_t  readn(int fd,void *ptr,size_t n);
ssize_t Readline(int fd, void *ptr, size_t maxlen);
ssize_t writen(int fd,const void *ptr,size_t n);
int Select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout);

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
int connect_nonblock_select(int sockfd,const struct sockaddr* addr,socklen_t addrlen,int nsec);
int connect_nonblock_epoll(int sockfd,const struct sockaddr* addr,socklen_t addrlen,int nsec);

typedef void (*sigFunc)(int);
sigFunc Signal(int sig,sigFunc func);

int tcp_connect(const char *host,const char *serv);
struct addrinfo* Host_serv(const char* host,const char* serv,int family,int socktype);

void *Malloc(size_t size);
void sig_chld(int signo);


#endif //UNP_UNP_H
