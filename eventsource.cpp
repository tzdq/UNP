//
// Created by Administrator on 2017/2/22.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <pthread.h>
#include <iostream>
#include <sys/epoll.h>
#include <signal.h>
#include <libgen.h>
#include <cmath>
#include <unistd.h>

using namespace std;

#define  MAX_EVENT_NUMBER 1024
static int pipefd[2];

int setnonblocking(int fd){
    int tmp = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,tmp|O_NONBLOCK);
    return tmp;
}

void addfd(int epollfd,int fd){
    epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblocking(fd);
}

void sig_handler(int sig){
    int save_errno = errno;
    int msg = sig;
    send(pipefd[1],(char*)&msg,1,0);
    errno = save_errno;
}

void addsig(int sig){
    struct  sigaction sa;
    memset(&sa,0, sizeof(sa));
    sa.sa_handler = sig_handler;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != -1);
}


int main(int argc,char* argv[]){
    if(argc <= 2 ){
        cout << "usage : " << basename(argv[0]) <<  "ip_address port number" << endl;
        return 1;
    }
    const char * ip = argv[1];
    int port = atoi(argv[2]);

    int ret  = 0 ;
    struct  sockaddr_in addr;
    int len = sizeof(addr);
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&addr.sin_addr);

    int listenfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    assert(listenfd > 2);

    ret = bind(listenfd,(struct sockaddr*) &addr,len);
    if(ret == -1){
        cout << "bind error" << endl;
        close(listenfd);
        return 1;
    }

    ret = listen(listenfd,5);
    if(ret == -1){
        cout<< "listen error" << endl;
        close(listenfd);
        return 1;
    }

    epoll_event events[MAX_EVENT_NUMBER];
    int epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd,listenfd);

    ret = socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
    assert(ret != -1);
    setnonblocking(pipefd[1]);
    addfd(epollfd,pipefd[0]);

    addsig(SIGHUP);
    addsig(SIGCHLD);
    addsig(SIGTERM);
    addsig(SIGINT);
    bool stop_server = false;
    while(!stop_server){
        int num = epoll_wait(epollfd,events,MAX_EVENT_NUMBER,-1);
        if((num < 0)&& ( errno != EINTR)){
            cout << "epoll_wait error"<<endl;
            break;
        }
        for(int i = 0 ; i != num; ++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){
                struct sockaddr_in clientAddr;
                socklen_t addrlen = sizeof(clientAddr);
                int clientfd = accept(listenfd,(struct sockaddr*)&clientAddr,&addrlen);
                if(clientfd < 0){
                    if(errno == EINTR){
                        --i;
                        continue;
                    }
                    else{
                        cout << "accept error"<<endl;
                        continue;
                    }
                }
                addfd(epollfd,clientfd);
            } else if((sockfd == pipefd[0]) && events[i].events & EPOLLIN){
                int sig;
                char signals[1024] = {0};
                ret = recv(pipefd[0],signals, sizeof(signals),0);
                if(ret <= 1)continue;
                else{
                    for(int i = 0 ;i < ret;++i){
                        switch (signals[i]){
                            case SIGCHLD:
                            case SIGHUP:{
                                continue;
                            }
                            case SIGTERM:
                            case SIGINT:{
                                stop_server = true;
                            }
                        }
                    }
                }
            }
            else{

            }
        }
    }
    cout << "close"<<endl;
    close(listenfd);
    close(epollfd);
    close(pipefd[0]);
    close(pipefd[1]);
    return 0;
}