//
// Created by Administrator on 2017/4/10.
//

#ifndef UNP_PROCESSPOOL_H
#define UNP_PROCESSPOOL_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
//#define IS_USE_EPOLL
//在支持epoll的linux环境编译时，记得把注释去掉，
//cygwin不支持epoll
#ifdef IS_USE_EPOLL
#include <sys/epoll.h>
#endif

#include <iostream>
using namespace std;

//描述一个子进程的类
class process{
public:
    process() :m_pid(-1){}

public:
    pid_t  m_pid;   //目标子进程的pid
    int m_pipefd[2];//父进程与子进程通信的管道
};

//进程池类，模板类，模板参数是处理逻辑任务的类
//单例模式
template <typename T>
class processpool{
private:
    processpool(int listenfd,int process_number = 8) ;

public:
    static processpool<T>* create(int listenfd,int process_number = 8){
        if(!m_instance){
            m_instance = new processpool<T>(listenfd,process_number);
        }
        return m_instance;
    }

    virtual ~processpool() {
        delete [] m_sub_process;
    }

    void run();//启动进程池

private:
    void run_parent();
    void run_child();
    void setup_sig_pipe();

private:
    static const int MAX_PROCESS_NUMBER = 16;//进程池最大子进程数目
    static const int USER_PER_PROCESS = 65536;//每个子进程最多能处理的客户数目
    static const int MAX_EVENT_NUMBER = 10000;//epoll最多能处理的事件数目

    int m_process_number;//进程池中的进程总数
    int m_idx;//子进程在进程池中的下标，从0开始
    int m_epollfd;//进程的epollfd
    int m_listenfd;//监听sockfd
    bool m_stop;//子进程是否停止运行的标志

    process *m_sub_process;//保存所有子进程的描述信息
    static processpool<T>* m_instance;//进程池静态实例
};

template <typename  T>
processpool<T>* processpool<T>::m_instance = NULL;

//用于统一事件源的信号管道
static int sig_pipefd[2];

//设置非阻塞函数
static int setnonblocking(int fd){
    int old = fcntl(fd,F_GETFL);
    fcntl(fd,F_SETFL,old|O_NONBLOCK);
    return old;
}

//epoll添加fd
static void addfd(int epollfd,int fd){
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    epoll_ctl(epollfd,EPOLL_CTL_ADD,fd,&ev);
    setnonblocking(fd);
}

//epoll 删除fd
static void removefd(int epollfd,int fd){
    epoll_ctl(epollfd,EPOLL_CTL_DEL,fd,0);
    close(fd);
}

//信号处理函数
static void sig_handler(int signo){
    int old_errno = errno;
    int msg = signo;
    send(sig_pipefd[1],(char*)&msg,1,0);
    errno = old_errno;
}

//信号处理函数设置
static void addsig(int sig, void(*hander)(int),bool restart = true){
    struct sigaction sa;
    memset(&sa,0, sizeof(sa));
    sa.sa_handler = hander;
    if(restart){
        sa.sa_flags |= SA_RESTART;
    }
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig,&sa,NULL) != -1);
}

//进程池构造函数，listenfd是监听套接字，必须在创建进程池前被创建，process_number是子进程的总数
template <typename  T>
processpool<T>::processpool(int listenfd, int process_number)
        : m_listenfd(listenfd),m_process_number(process_number)
        ,m_idx(-1),m_stop(false),m_epollfd(-1){
    assert((process_number > 0 ) && (process_number <= MAX_PROCESS_NUMBER));

    m_sub_process = new process[process_number];
    assert(m_sub_process);

    for(int i = 0 ; i < process_number;++i){
        int ret = socketpair(AF_UNIX,SOCK_STREAM,0,m_sub_process[i].m_pipefd);
        assert(ret == 0);

        m_sub_process[i].m_pid = fork();
        assert(m_sub_process[i].m_pid >= 0);
        if(m_sub_process[i].m_pid > 0){
            close(m_sub_process[i].m_pipefd[1]);
            continue;
        }
        else
        {
            close(m_sub_process[i].m_pipefd[0]);
            m_idx = i;
            break;
        }
    }
}


template <typename T>
void processpool<T>::setup_sig_pipe() {
    //创建epoll事件监听表和信号管道
    m_epollfd = epoll_create(5);
    assert(m_epollfd != -1);

    int ret = socketpair(AF_UNIX,SOCK_STREAM,0,sig_pipefd);
    assert(ret == 0);

    setnonblocking(sig_pipefd[1]);
    addfd(m_epollfd,sig_pipefd[0]);

    addsig(SIGCHLD,sig_handler);
    addsig(SIGTERM,sig_handler);
    addsig(SIGINT,sig_handler);
    addsig(SIGPIPE,SIG_IGN);
}


//父进程中m_idx为-1，子进程中大于等于0
template <typename  T>
void processpool<T>::run() {
    if(m_idx != - 1){
        run_child();
        return;
    }
    run_parent();
}

template <typename T>
void processpool<T>::run_child() {
    setup_sig_pipe();

    //每个子进程中都通过其在进程池中的序号值m_idx找到与父进程通信的管道
    int pipefd = m_sub_process[m_idx].m_pipefd[1];
    //子进程需要监听管道文件描述符pipefd，父进程通过它来通知子进程accept新连接
    addfd(m_epollfd,pipefd);

    epoll_event events[MAX_EVENT_NUMBER];
    T* user = new T[USER_PER_PROCESS];
    assert(user);
    int number = 0;
    int ret = -1;
    while (!m_stop){
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR){
            cout << "epoll error" << endl;
            break;
        }
        for(int i = 0 ;i < number;++i){
            int sockfd = events[i].data.fd;
            if(sockfd == pipefd && events[i].events & EPOLLIN){
                int client = 0;
                //从父子进程中的管道中读取数据，并保存在client中，如果读取成功表示有新连接到来
                ret = recv(sockfd,(char*)&client, sizeof(client),0);//
                if((ret < 0 && (errno != EAGAIN )) || (ret == 0)){
                    cout << "recv error" << endl;
                    continue;
                }
                struct sockaddr_in addr;
                socklen_t addrlen = sizeof(addr);
                int connfd = accept(m_listenfd,(struct sockaddr*)&addr,&addrlen);
                if(connfd < 0){
                    cout << "accept error" << endl;
                    continue;
                }
                addfd(m_epollfd,connfd);
                //模板类必须提供init方法，以初始化一个客户连接，我们直接使用connfd来索引逻辑处理对象，提高程序效率
                user[connfd].init(m_epollfd,connfd,addr);
            }
            else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN){//信号事件
                char signals[1024] = {0};
                ret = recv(sockfd,signals, sizeof(signals),0);
                if(ret <= 0){
                    continue;
                }
                else{
                    for(int  i = 0 ; i < ret;++i) {
                        switch (signals[i]) {
                            case SIGCHLD:
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG) > 0)) {
                                    continue;
                                }
                                break;
                            case SIGTERM:
                            case SIGINT:
                                m_stop = true;
                                break;
                            default:
                                break;
                        }
                    }
                }
            }
            else if(events[i].events & EPOLLIN){//如果有其它可读事件，绝对是客户请求到来
                user[sockfd].process();//模板T必须包含process函数
            }
            else {
                continue;
            }
        }
    }
    delete []user;
    user = NULL;
    close(pipefd);
    close(m_epollfd);
    //m_listenfd应该由创建者光闭
}

template <typename  T>
void processpool<T>::run_parent() {
    setup_sig_pipe();

    addfd(m_epollfd,m_listenfd);

    epoll_event events[MAX_EVENT_NUMBER];
    int sub_process_counter = 0;
    int new_conn = 1;
    int number = 0;
    int ret = -1;
    while (!m_stop){
        number = epoll_wait(m_epollfd,events,MAX_EVENT_NUMBER,-1);
        if(number < 0 && errno != EINTR){
            cout << "epoll error" << endl;
            break;
        }
        for(int i = 0 ;i < number;++i){
            int sockfd = events[i].data.fd;
            if(sockfd == m_listenfd){
                int idx = sub_process_counter;
                do{
                    if(m_sub_process[idx].m_pid != -1){
                        break;
                    }
                     idx = (idx+1)%m_process_number;
                }while(idx != sub_process_counter);
                if(m_sub_process[idx].m_pid == -1){
                    m_stop = true;
                    break;
                }
                sub_process_counter = (idx +1)% m_process_number;
                send(m_sub_process[idx].m_pipefd[0],(char*)&new_conn, sizeof(new_conn),0);
                cout <<"send request to child "<<idx<<endl;
            }
            else if(sockfd == sig_pipefd[0] && events[i].events & EPOLLIN) {//信号事件
                char signals[1024] = {0};
                ret = recv(sockfd, signals, sizeof(signals), 0);
                if (ret <= 0) {
                    continue;
                }
                else{
                    for (int i = 0; i < ret; ++i) {
                        switch (signals[i]) {
                            case SIGCHLD:
                                pid_t pid;
                                int stat;
                                while ((pid = waitpid(-1, &stat, WNOHANG) > 0)) {
                                    for (int j = 0; j < m_process_number; ++j) {
                                    //如果第i个子进程已经退出了，逐渐从关闭相应的通信管道，设置想应的pid为-1，标记该子进程退出
                                        if (m_sub_process[j].m_pid == pid) {
                                            cout << "child " << j << "join" << endl;
                                            close(m_sub_process[j].m_pipefd[0]);
                                            m_sub_process[j].m_pid = -1;
                                        }
                                    }
                                }

                            //如果所有子进程都退出了，父进程退出
                                m_stop = true;
                                for (int j = 0; j < m_process_number; ++j) {
                                    if (m_sub_process[j].m_pid != -1) {
                                        m_stop = false;
                                    }
                                }
                                break;
                            case SIGTERM:
                            case SIGINT:
                                    cout << "kill all child process now" << endl;
                                    for (int j = 0; j < m_process_number; ++j) {
                                        int pid = m_sub_process[j].m_pid;
                                        if (pid != -1) {
                                            kill(pid, SIGTERM);
                                        }
                                    }
                                    break;
                            default:
                                break;
                        }
                    }
                }
            }
            else {
                continue;
            }
        }
    }

    close(m_epollfd);
    //m_listenfd应该由创建者光闭
}
#endif //UNP_PROCESSPOOL_H
