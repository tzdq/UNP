//
// Created by Administrator on 2017/3/30.
//

#include "unp.h"
#include <sys/epoll.h>
#include <fcntl.h>

#define  MAX_EVENT_NUM 1024

void addfd(int epollfd,int fd,bool hup = false){
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    if(hup){
        ev.events |= EPOLLRDHUP;
    }
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
}

int main(int argc,char **argv){
    if(argc <= 2){
        cout << "usgae : "<<basename(argv[0]) << "ipaddr port" << endl;
        return 1;
    }

    const  char* ip = argv[1];
    int port = atoi(argv[2]);

    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&addr.sin_addr);

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    assert(sockfd >= 0) ;

    int ret = connect(sockfd,(struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0){
        cout << "connect error"<<endl;
        close(sockfd);
        return 1;
    }

    int epollfd = epoll_create(5);
    assert(epollfd != 0);
    struct epoll_event events[MAX_EVENT_NUM];

    addfd(epollfd,sockfd,true);
    addfd(epollfd,STDIN_FILENO);

    char buf[MAX_BUFF_SIZE] = {0};
    int pipefd[2];
    ret = pipe(pipefd);
    assert(ret != -1);
    int fd;

    while(1){
        ret = epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if(ret < 0){
            cout << "epoll_wait error"<<endl;
            break;
        }
        for(int i = 0 ;i < ret;i++){
            fd = events[i].data.fd;
            if(fd == sockfd && events[i].events & EPOLLRDHUP){
                cout << "connect closed by peer" << endl;
                break;
            }
            else if(fd == sockfd && events[i].events & EPOLLIN){
                memset(buf,0,MAX_BUFF_SIZE);
                recv(sockfd,buf,MAX_BUFF_SIZE-1,0);
                cout << "recv buf :" << buf<<endl;
            }
            else if(fd == 0 && events[i].events &EPOLLIN){
                ret = splice(0,NULL,pipefd[1],NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
                ret = splice(pipefd[0],NULL,sockfd,NULL,32768,SPLICE_F_MORE|SPLICE_F_MOVE);
            }
        }
    }
    close(sockfd);
    return 0;
}


