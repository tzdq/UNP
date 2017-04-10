//
// Created by Administrator on 2017/3/25.
//

#include "unp.h"
#include "listtimer.h"

#define  FD_LIMIT 65535
#define  MAX_EVENT_NUM 1024
#define  TIMESLOT       5

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd;

void add_fd(int epollfd,int fd){
    struct epoll_event event;
    event.data.fd = fd;
    event.events = EPOLLIN|EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&event);
    setnonblock(fd);
}
void sig_handle(int sig){
    int tmp_errno = errno;
    int msg = sig;
    send(pipefd[1],(char*)&msg,1,0);
    errno = tmp_errno;
}

void add_sig(int sig){
    struct sigaction sa;
    memset(&sa,0, sizeof(sa));
    sa.sa_handler = sig_handle;
    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    sigaction(sig,&sa,NULL);
}

void timer_handler(){
    timer_lst.tick();
    alarm(TIMESLOT);
}

void cb_func(client_data *user_data){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,user_data->sockfd,0);
    assert(user_data);
    cout << "close fd = " << user_data->sockfd << endl;
    close(user_data->sockfd);
}

int main(int argc,char **argv){
    if(argc < 2){
        cout << "usage: " << basename(argv[0]) << "ip_address port"<<endl;
        return 1;
    }

    const  char *ip = argv[1];
    int port = atoi(argv[2]);

    int ret = 0;
    struct sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8888);
    inet_pton(AF_INET,ip,&addr.sin_addr);

    int sockfd = socket(AF_INET,SOCK_STREAM,0);
    if(sockfd < 0){
        err_sys("socket error");
    }

    ret  = bind(sockfd,(struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0){
        close(sockfd);
        err_sys("bind error");
    }

    ret = listen(sockfd,LISTENQ);
    assert(ret != -1);

    epoll_event events[MAX_EVENT_NUM];
    int epollfd = epoll_create(6);
    assert(epollfd != -1);
    add_fd(epollfd,sockfd);

    ret =socketpair(PF_UNIX,SOCK_STREAM,0,pipefd);
    assert(ret != -1);
    setnonblock(pipefd[1]);
    add_fd(epollfd,pipefd[0]);

    add_sig(SIGTERM);
    add_sig(SIGALRM);
    bool stop_server = false;

    client_data *users = new client_data[FD_LIMIT];
    bool timeout = false;
    alarm(TIMESLOT);

    while(!stop_server){
        int num = epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if((num < 0) && (errno != EINTR)){
            err_sys("epoll_wait error");
            break;
        }

        for(int i = 0 ;i < num;i++){
            int fd = events[i].data.fd;
            if(fd == sockfd){
                struct sockaddr_in client_addr;
                socklen_t  len = sizeof(client_addr);
                int conn = accept(sockfd,(struct sockaddr*)&client_addr,&len);
                add_fd(epollfd,conn);
                users[conn].sockfd = conn;
                users[conn].address = client_addr;
                util_timer *timer = new util_timer;
                timer->cb_func = cb_func;
                time_t cur = time(NULL);
                timer->expire = cur + 3* TIMESLOT;
                timer->user_data =&users[conn];
                users[conn].timer = timer;
                timer_lst.add_timer(timer);
            }
            else if(fd == pipefd[0] && events[i].events & EPOLLIN){
                char buf[MAX_BUFF_SIZE] = {0};
                ret = recv(fd,buf,MAX_BUFF_SIZE,0);
                if(ret < 0 ){
                    //handle error
                    continue;
                }
                else if (ret == 0)continue;
                else{
                    for(int i = 0; i < ret;i++){
                        switch (buf[i]){
                            case SIGALRM:
                                timeout = true;
                                break;
                            case SIGTERM:
                                stop_server = true;
                                break;
                        }
                    }
                }
            } else if(events[i].events & EPOLLIN){
                memset(users[fd].buf,0,MAX_BUFF_SIZE);
                util_timer *timer = users[fd].timer;
                ret = recv(fd,users[fd].buf,MAX_BUFF_SIZE-1,0);
                if(ret < 0){
                    if(errno != EAGAIN ){
                        cb_func(&users[fd]);
                        if(timer){
                            timer_lst.delete_timer(timer);
                        }
                    }
                }else if(ret == 0){
                    cb_func(&users[fd]);
                    if(timer)timer_lst.delete_timer(timer);
                }else{
                    cout << "recv "<< ret <<"bytes data " << users[fd].buf << "from " << fd<<endl;
                    if(timer){
                        time_t  cur = time(NULL);
                        timer->expire = cur + 3*TIMESLOT;
                        cout <<"timer adjust"<<endl;
                        timer_lst.adjust_timer(timer);
                    }
                }
            }
            else {}
        }
        if(timeout){
            timer_handler();
            timeout = false;
        }
    }

    close(sockfd);
    close(epollfd);
    close(pipefd[0]);
    close(pipefd[1]);
    delete []users;

    return 0;
}