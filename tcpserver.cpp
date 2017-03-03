//
// Created by Administrator on 2017/2/24.
//

#include "unp.h"
using namespace std;

void str_echo(int sockfd){
    ssize_t  n;
    char buf[MAX_BUFF_SIZE] = {0};
again:
    while((n = read(sockfd,buf,MAX_BUFF_SIZE)) > 0){
        Writen(sockfd,buf,n);
    }
    if(n < 0 && errno == EINTR)
        goto again;
    else if(n < 0)
        err_sys("str_echo:read echo");
}

void sig_chld(int sig){
    pid_t  pid;
    int stat;
    while((pid = waitpid(-1,&stat,WNOHANG)) > 0)
        cout <<"child " << pid << "terminated"<<endl;
    return;
}
#if 0
int main(int argc,char **argv){
    int listenfd,connfd;
    pid_t  childpid;
    socklen_t  clilen;

    listenfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    assert(listenfd > 2);

    struct sockaddr_in cliaddr,servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_pton(AF_INET,INADDR_ANY,&servaddr.sin_addr);//这儿不能这么写

    int ret = bind(listenfd,(struct sockaddr*)&servaddr, sizeof(servaddr));
    assert(ret !=-1);

    ret = listen(listenfd,5);
    assert(ret != -1);

    Signal(SIGCHLD,sig_chld);
    for(;;){
        clilen = sizeof(cliaddr);
        connfd = accept(listenfd,(struct sockaddr*)&cliaddr,&clilen);
        if(connfd < 0){
            if(errno == EINTR)continue;
            else err_sys("accept error");
        }
        if((childpid = fork()) == 0){
            close(listenfd);
            str_echo(connfd);
            close(connfd);
            exit(0);
        }
        close(connfd);
    }

    return 0;
}
#else
int xmain(int argc,char **argv){
    int listenfd,connfd,sockfd;
    int maxfd,maxi,i;
    int nReady,client[FD_SETSIZE];
    socklen_t  clilen;
    fd_set rset,allset;
    ssize_t n;
    char buf[MAX_BUFF_SIZE] = {0};

    listenfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    assert(listenfd > 2);

    struct sockaddr_in cliaddr,servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //inet_pton(AF_INET,INADDR_ANY,&servaddr.sin_addr);//这儿不能这么写

    int ret = bind(listenfd,(struct sockaddr*)&servaddr, sizeof(servaddr));
    assert(ret !=-1);

    ret = listen(listenfd,5);
    assert(ret != -1);

    maxfd = listenfd;
    maxi = -1;
    for(int  i = 0 ;i < FD_SETSIZE;++i)
        client[i] = -1;
    FD_ZERO(&allset);
    FD_SET(listenfd,&allset);

    for(;;){
        rset = allset;
        nReady = select(maxfd+1,&rset,NULL,NULL,NULL);
        if(nReady == 0)continue;
        else if(nReady < 0 )err_sys("select error");
        else
        {
            if(FD_ISSET(listenfd,&rset)){
                clilen = sizeof(cliaddr);
                connfd = accept(listenfd,(struct sockaddr*)&cliaddr,&clilen);
                if(connfd < 0){
                    if(errno == EINTR)continue;
                    else err_sys("accept error");
                }

                for (i = 0; i < FD_SETSIZE; ++i) {
                    if(client[i] < 0){
                        client[i] = connfd;
                        break;
                    }
                }
                if (i == FD_SETSIZE){
                    err_quit("too much connect");
                }

                FD_SET(connfd,&allset);
                if(connfd > maxfd)
                    maxfd = connfd;
                if(i > maxi)
                    maxi = i;
                if(--nReady <= 0)continue;
            }
            for (int j = 0; j <= maxi; ++j) {
                if((sockfd = client[j]) < 0)continue;
                if(FD_ISSET(sockfd,&rset)){
                    if((n = read(sockfd,buf,MAX_BUFF_SIZE)) == 0){
                        close(sockfd);
                        FD_CLR(sockfd,&allset);
                        client[i] = -1;
                    }
                    else{
                        Writen(sockfd,buf,n);
                    }
                    if(--nReady <= 0)break;
                }
            }
        }
    }

    close(listenfd);
    for (i = 0; i < FD_SETSIZE; ++i) {
        if(client[i]  < 0)continue;
        close(client[i]);
    }
    return 0;
}
#endif
