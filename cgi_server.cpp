//
// Created by Administrator on 2017/4/10.
//

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
#include "processpool.h"
#ifdef IS_USE_EPOLL
#include <sys/epoll.h>
#endif

#include <iostream>
#include <libgen.h>

using namespace std;

class cgi_conn{
public:
    cgi_conn() {}
    virtual ~cgi_conn() {}

    void init(int epollfd,int sockfd,const sockaddr_in &clientAddr){
        m_epollfd = epollfd;
        m_sockfd = sockfd;
        m_address = clientAddr;
        memset(m_buf,0, sizeof(m_buf));
        m_read_idx = 0;
    }

    void process(){
        int idx = 0;
        int ret = -1;
        while(1){
            idx = m_read_idx;
            ret = recv(m_sockfd,m_buf+idx,BUFFER_SIZE - idx - 1,0);
            if(ret < 0){
                if(errno != EAGAIN){
                    removefd(m_epollfd,m_sockfd);
                }
                break;
            }
            else if(ret == 0){//对端关闭，服务器也关闭
                removefd(m_epollfd,m_sockfd);
                break;
            }
            else{
                m_read_idx += ret;
                cout << "user content is:" <<m_buf<<endl;
                //如果遇到"\r\n"，则开始处理用户数据
                for (; idx < m_read_idx; ++idx) {
                    if((idx >= 1) &&m_buf[idx-1] == '\r' &&m_buf[idx] == '\n'){
                        break;
                    }
                }
                //如果没有"\r\n"，表示需要更多数据
                if(idx == m_read_idx)continue;

                m_buf[idx-1] = '\0';

                char *file_name = m_buf;
                //判断用户要运行的CGI程序是否存在
                if(access(file_name,F_OK) == -1){
                    removefd(m_epollfd,m_sockfd);
                    break;
                }

                ret =fork();
                if(ret == -1){
                    removefd(m_epollfd,m_sockfd);
                    break;
                }
                else if(ret == 0){
                    //子进程将标准输出定向到m_sockfd，执行CGI
                    close(STDOUT_FILENO);
                    dup(m_sockfd);
                    execl(m_buf,m_buf,0);
                    exit(0);
                }
                else{
                    //父进程只要关闭连接
                    removefd(m_epollfd,m_sockfd);
                    break;
                }
            }
        }
    }

private:
    static const int BUFFER_SIZE = 1024;//读缓冲区的大小
    static int m_epollfd ;
    int m_sockfd;
    sockaddr_in m_address;
    char m_buf[BUFFER_SIZE];
    int m_read_idx;//标记读缓冲区的已经读入的数据的下一个字节的位置
};

int cgi_conn::m_epollfd = -1;

int main(int argc,char**argv){
    if(argc <= 2){
        cout << "usage :"<<basename(argv[0]) << "ip_address port"<<endl;
        return 1;
    }
    const char *ip = argv[1];
    int port = atoi(argv[2]);

    int listenfd = socket(AF_INET,SOCK_STREAM,0);
    assert(listenfd >= 0);

    int ret = 0;
    struct sockaddr_in addr;
    memset(&addr,0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET,ip,&addr.sin_addr);

    ret = bind(listenfd,(struct sockaddr*)&addr, sizeof(addr));
    assert(ret != -1);

    ret = listen(listenfd,5);
    assert(ret != -1);

    processpool<cgi_conn>* pool  = processpool<cgi_conn>::create(listenfd);
    if(pool){
        pool->run();
        delete pool;
    }
    close(listenfd);
    return 0;
}