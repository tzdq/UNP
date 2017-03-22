//
// Created by Administrator on 2017/3/6.
//

#include "web.h"
#include <iostream>
#include <w32api/ws2tcpip.h>

using namespace std;

//获取主页的数据
void home_page(const char *host,const char *fname){
    int fd = tcp_connect(host,SERV_PORT);

    char buf[MAX_BUFF_SIZE] = {0};
    int n = snprintf(buf,sizeof(buf),GET_CMD,fname);
    Writen(fd,buf,n);

    for(;;){
        memset(buf,0, sizeof(buf));
        if((n = read(fd,buf,MAX_BUFF_SIZE)) == 0)break;
        cout << "read "<<n << "bytes of home_page"<<endl;
    }
    cout << "end-of file on home page"<<endl;
    close(fd);
}

void start_connect(struct File *fptr){
    int fd,n;
    struct  addrinfo *ai;
    ai = Host_serv(fptr->host,SERV_PORT,0,SOCK_STREAM);

    fd = socket(ai->ai_family,ai->ai_socktype,ai->ai_protocol);
    if(fd < 0)
        err_sys("socket error");

    fptr->fd = fd;
    cout << "start_connect for "<<fptr->name << ",fd " << fd <<endl;

    setnonblock(fd);

    if((n = connect(fd,ai->ai_addr,ai->ai_addrlen)) < 0){
        if(errno != EINPROGRESS)
            err_sys("nonblocking connect error");
        fptr->flags = F_CONNECTING;

        FD_SET(fd,&g_rSet);
        FD_SET(fd,&g_wSet);
        if(fd > g_maxFd)
            g_maxFd = fd;
    }
    else if(n >= 0) write_get_cmd(fptr);
}
void write_get_cmd(struct File *fptr){
    int n;
    char buf[MAX_BUFF_SIZE] = {0};

    n = snprintf(buf,MAX_BUFF_SIZE,GET_CMD,fptr->name);
    Writen(fptr->fd,buf,n);
    cout << "wrote "<< n << "bytes for " << fptr->name << endl;
    fptr->flags = F_READING;

    FD_SET(fptr->fd,&g_rSet);
    if(fptr->fd > g_maxFd)
        g_maxFd = fptr->fd;
}

int main(int argc,char **argv){
    if(argc < 5){
        err_quit("usage : web [conns] [hostname] [homepage] [file1] ...");
    }

    int fd,maxConn,nFiles;
    int i,n,flags,error;
    char buf[MAX_BUFF_SIZE] = {0};
    fd_set rs,ws;
    maxConn = atoi(argv[1]);
    nFiles = min(argc -4,MAX_FILE_NUM);
    for (i = 0; i < nFiles; ++i) {
        g_file[i].host = argv[2];
        g_file[i].name = argv[i+4];
        g_file[i].flags = 0;
    }
    cout << "nFiles = " << nFiles << endl;

    home_page(argv[2],argv[3]);

    FD_ZERO(&g_rSet);
    FD_ZERO(&g_wSet);

    g_maxFd = -1;
    g_nLeftToConn = g_nLeftToRead = nFiles;
    g_nConn = 0;

    while(g_nLeftToRead > 0){
        while(g_nConn < maxConn && g_nLeftToConn > 0){
            for(i = 0; i <nFiles;++i){
                if(g_file[i].flags == 0)
                    break;
            }
            if(i == nFiles)
                err_quit("nLeftToConn = %d,but nothing found",g_nLeftToConn);
            start_connect(&g_file[i]);
            g_nConn++;
            g_nLeftToConn--;
        }

        rs = g_rSet;
        ws = g_wSet;
        n = Select(g_maxFd+1,&rs,&ws,NULL,NULL);

        for(i = 0; i < n ;i++){
            flags = g_file[i].flags;
            if(flags == 0 || flags == F_DONE)continue;
            fd = g_file[i].fd;
            if(flags & F_CONNECTING && (FD_ISSET(fd,&rs) || FD_ISSET(fd,&ws))){
                n = sizeof(error);
                if(getsockopt(fd,SOL_SOCKET,SO_ERROR,&error,&n) < 0 || error != 0)
                    err_ret("nonblocking connect failed for %s",g_file[i].name);
                cout << "connection established for" << g_file[i].name << endl;
                FD_CLR(fd,&g_wSet);
                write_get_cmd(&g_file[i]);
            }
            else if (flags & F_READING && FD_ISSET(fd,&rs)){
                if((n = read(fd,buf,MAX_BUFF_SIZE)) == 0){
                    cout << "End-of-file on" << g_file[i].name << endl;
                    close(fd);
                    g_file[i].flags = F_DONE;
                    FD_CLR(fd,&g_rSet);
                    g_nConn--;
                    g_nLeftToRead--;
                }
                else cout << "read "<< n << "bytes from " << g_file[i].name << endl;
            }
        }
    }

    return 0;
}


