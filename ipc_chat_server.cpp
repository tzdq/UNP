//
// Created by Administrator on 2017/3/30.
//

#include "unp.h"
#include <sys/shm.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/epoll.h>
#include <signal.h>

#define  USER_LIMIT    5
#define  BUFFER_SIZE   1024
#define  FD_LIMIT       65535
#define  MAX_EVENT_NUM 1024
#define  PROCESS_LIMIT 65536

struct ipc_client_data{
    sockaddr_in addr;//
    int connfd;//
    pid_t pid;//处理这个连接的子进程ID
    int pipefd[2];//和父进程通信用的管道
};

static const char* shm_name = "/my_shm";
int sig_pipefd[2];//信号的管道
int epollfd;//epoll句柄
int listenfd;//
int shmfd;//
char* share_mem = 0;
bool stop_child = false;

int user_count;//当前客户数量
ipc_client_data* users = 0;//客户连接数组，进程用客户连接的编号来索引这个数组，即可取得相关的客户连接数据
int* sub_process = 0;//子进程和客户连接的映射关系，用进程的PID来索引这个数组，即可取得该进程所处理的客户连接的编号

void addfd(int epollfd,int fd){
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN|EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
    setnonblock(fd);
}

void sig_handler(int sig){
    int tmp = errno;
    int msg = sig;
    send(sig_pipefd[1],(char*)&msg,1,0);
    errno = tmp;
}

void addsig(int sig,void(*handler)(int),bool restart = true){
    struct sigaction si;
    memset(&si,0,sizeof(si));
    si.sa_handler = handler;
    if(restart){
        si.sa_flags |= SA_RESTART;
    }
    sigfillset(&si.sa_mask);

    sigaction(sig,&si,NULL);
}

void del_resource(){
    close(sig_pipefd[0]);
    close(sig_pipefd[1]);
    close(listenfd);
    close(epollfd);
    shm_unlink(shm_name);
    delete []users;
    delete []sub_process;
    munmap((void*)share_mem,USER_LIMIT*BUFFER_SIZE);
}

void child_term_handler(int sig){
    stop_child = true;
}

int run_child(int idx,ipc_client_data *user,char *share_mem){
    epoll_event events[MAX_EVENT_NUM];
    int child_epollfd = epoll_create(5);
    assert(child_epollfd != -1);

    int connfd = user[idx].connfd;
    addfd(child_epollfd,connfd);

    int pipefd = user[idx].pipefd[1];//写端
    addfd(child_epollfd,pipefd);

    int ret = 0;
    addsig(SIGTERM,child_term_handler,false);
    while(!stop_child){
        int number = epoll_wait(child_epollfd,events,MAX_EVENT_NUM,-1);
        if(number < 0 && errno != EINTR){
            cout << "epoll_wait error"<<endl;
            break;
        }
        for(int i = 0 ;i < number;i++){
            int sockfd = events[i].data.fd ;
            if(sockfd == connfd && events[i].events & EPOLLIN){
                memset(share_mem + idx * BUFFER_SIZE,0,BUFFER_SIZE);
                ret = recv(connfd,share_mem+idx*BUFFER_SIZE,BUFFER_SIZE-1,0);
                if(ret < 0){
                    if(errno != EAGAIN)stop_child = true;
                }
                else if(ret == 0)stop_child = true;
                else {
                    send(pipefd,(char*)&idx, sizeof(idx),0);
                }
            }
            else if(sockfd == pipefd && events[i].events & EPOLLIN){
                int client;
                ret = recv(sockfd,(char*)&client,sizeof(client),0);
                if(ret < 0){
                    if(errno != EAGAIN)stop_child = true;
                }
                else if(ret == 0)stop_child = true;
                else {
                    send(connfd,share_mem+client*BUFFER_SIZE, sizeof(idx),0);
                }
            }
            else continue;
        }
    }
    close(child_epollfd);
    close(pipefd);
    close(connfd);
    return 0;
}

int main(int argc,char **argv){
    if(argc <= 2){
        cout << "usage :"<<basename(argv[0]) << " ipaddr port" << endl;
        return 1;
    }

    const char* ip = argv[1];
    int port = atoi(argv[2]);

    int  ret = 0;
    sockaddr_in addr;
    bzero(&addr, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&addr.sin_addr);

    listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0 );

    const int on = 1;
    ret = setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,&on, sizeof(on));
    assert(ret !=-1);

    ret = bind(listenfd,(struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd,5);
    assert(ret != -1);

    user_count = 0;
    users = new ipc_client_data[USER_LIMIT + 1];
    sub_process = new int[PROCESS_LIMIT];
    for (int i = 0; i < PROCESS_LIMIT; ++i) {
        sub_process[i] = -1;
    }

    epoll_event events[MAX_EVENT_NUM];

    epollfd = epoll_create(5);
    assert(epollfd != -1);
    addfd(epollfd,listenfd);

    ret =socketpair(PF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret != -1);
    setnonblock(sig_pipefd[1]);
    addfd(epollfd,sig_pipefd[0]);

    addsig(SIGCHLD,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGINT,sig_handler);
    addsig(SIGPIPE,SIG_IGN);

    bool stop_server = false;
    bool terminate = false;

    shmfd = shm_open(shm_name,O_CREAT|O_RDWR,0666);
    assert(shmfd != -1);

    ret = ftruncate(shmfd,USER_LIMIT*BUFFER_SIZE);
    assert(ret != -1);

    share_mem = (char*)mmap(NULL,USER_LIMIT*BUFFER_SIZE,PROT_READ|PROT_WRITE,MAP_SHARED,shmfd,0);
    assert(share_mem != MAP_FAILED);

    close(shmfd);

    while(!stop_server){
        int number = epoll_wait(epollfd,events,MAX_EVENT_NUM,-1);
        if(number < 0 && errno != EINTR){
            cout << "epoll_wait error"<<endl;
            break;
        }
        for(int i = 0 ;i < number;++i){
            int sockfd = events[i].data.fd;
            if(sockfd == listenfd){//新连接到来
                struct sockaddr_in client_addr;
                socklen_t  client_len = sizeof(client_addr);
                int connfd = accept(listenfd,(struct sockaddr*)&client_addr,&client_len);
                if(connfd < 0){
                    cout << "accept error" <<endl;
                    continue;
                }
                if(user_count >= USER_LIMIT){
                    const char *msg = "too much user\n";
                    cout << msg;
                    send(connfd,msg,strlen(msg),0);
                    close(connfd);
                    continue;
                }

                cout <<"recv a new conn,fd = "<<connfd<<",ip = " << inet_ntoa(client_addr.sin_addr)<<endl;
                users[user_count].addr = client_addr;
                users[user_count].connfd = connfd;
                ret = socketpair(PF_UNIX,SOCK_STREAM,0,users[user_count].pipefd);
                assert(ret != -1);

                pid_t  pid = fork();
                if(pid < 0){
                    close(connfd);
                    continue;
                }
                else if(pid == 0){
                    close(epollfd);
                    close(listenfd);
                    close(users[user_count].pipefd[0]);
                    close(sig_pipefd[0]);
                    close(sig_pipefd[1]);
                    run_child(user_count,users,share_mem);
                    munmap((void*)share_mem,USER_LIMIT*BUFFER_SIZE);
                    exit(0);
                } else if(pid > 0){
                    close(connfd);
                    close(users[user_count].pipefd[1]);
                    addfd(epollfd,users[user_count].pipefd[0]);
                    users[user_count].pid = pid;
                    sub_process[pid] = user_count;
                    user_count++;
                    continue;
                }
            }
            else if(sockfd == sig_pipefd[0] && events[i].events &EPOLLIN){
                    char sig_buf[BUFFER_SIZE] = {0};
                    ret = recv(sig_pipefd[0],sig_buf,BUFFER_SIZE,0);
                if(ret < 0){
                    continue;
                }
                else if(ret == 0){
                    continue;
                }
                else {
                    for(int i = 0 ;i < ret ;++i){
                        switch (sig_buf[i]){
                            case SIGTERM:
                            case SIGINT:
                                cout <<"kill all child now" << endl;
                                if(user_count == 0){
                                    stop_server = true;
                                    break;
                                }
                                for(int i = 0;i<user_count;++i){
                                    int pid = users[i].pid;
                                    kill(pid,SIGTERM);
                                }
                                terminate = true;
                                break;
                            case SIGCHLD:
                                int stat;
                                pid_t  pid;
                                while((pid = waitpid(-1,&stat,WNOHANG)) > 0){
                                    cout << "child stoped! pid = " << pid<<endl;
                                    int del_user = sub_process[pid];
                                    sub_process[pid] = -1;
                                    if(del_user < 0 || del_user > USER_LIMIT)continue;
                                    epoll_ctl(epollfd,EPOLL_CTL_DEL,users[del_user].pipefd[0],0);
                                    close(users[del_user].pipefd[0]);
                                    users[del_user] = users[--user_count];
                                    sub_process[users[del_user].pid] = user_count;
                                }
                                if(terminate && user_count == 0){
                                    stop_server = true;
                                }
                                break;
                            default:break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN){
                int child = 0 ;
                ret = recv(sockfd,(char*)&child,sizeof(child),0);
                if(ret <= 0){
                    continue;
                }
                cout << "read data from child accross pipe"<<endl;
                for(int j = 0 ;j < user_count;j++){
                    if(users[j].pipefd[0] != sockfd){
                        cout << "send data to child accross pipe"<<endl;
                        send(users[j].pipefd[0],(char*)&child, sizeof(child),0);
                     }
                }
            }
        }
    }

    del_resource();
    return 0;
}