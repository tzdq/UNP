//
// Created by Administrator on 2017/2/24.
//

#include "unp.h"
using namespace std;

//select 版本
void str_cli_select(FILE* fp,int sockfd){
    int maxfd ,stdineof,n;
    fd_set rset;
    char recvbuf[MAX_BUFF_SIZE] = {0};
    char sendbuf[MAX_BUFF_SIZE] = {0};

    stdineof = 0;
    for (;;){
        FD_ZERO(&rset);
        if(stdineof == 0)
            FD_SET(fileno(fp),&rset);
        FD_SET(sockfd,&rset);
        maxfd = max(fileno(fp),sockfd) +1;
        int ret = select(maxfd,&rset,NULL,NULL,NULL);
        if(ret == 0)continue;
        else if(ret < 0 ) {
            err_sys("select error");
        }
        else{
            if(FD_ISSET(fileno(fp),&rset)){
                memset(sendbuf,0,MAX_BUFF_SIZE);
                if((n = read(fileno(fp),sendbuf,MAX_BUFF_SIZE)) == 0){
                    stdineof = 1;
                    shutdown(sockfd,SHUT_WR);
                    FD_CLR(fileno(fp),&rset);
                    continue;
                }
                Writen(sockfd,sendbuf,strlen(sendbuf));
            }
            if(FD_ISSET(sockfd,&rset)){
                memset(recvbuf,0,MAX_BUFF_SIZE);
                if((n = read(sockfd,recvbuf,MAX_BUFF_SIZE)) == 0){
                    if(stdineof == 1)return;
                    else err_quit("str_cli : server terminated prematurely");
                }
                Writen(fileno(fp),recvbuf,n);
            }
        }
    }
}

void str_cli_nonblock(FILE* fp,int sockfd){
    int maxfd ,stdineof,n;
    fd_set rset,wset;
    char recvbuf[MAX_BUFF_SIZE] = {0};
    char sendbuf[MAX_BUFF_SIZE] = {0};
    char *recviptr,*recvoptr,*sendiptr,*sendoptr;

    setnonblock(sockfd);
    setnonblock(STDIN_FILENO);
    setnonblock(STDOUT_FILENO);

    recviptr = recvoptr = recvbuf;
    sendiptr = sendoptr = sendbuf;

    stdineof = 0;

    maxfd = max(STDOUT_FILENO,sockfd)+1;
    for (;;){
        FD_ZERO(&rset);
        FD_ZERO(&wset);

        if(stdineof == 0 && sendiptr < &sendbuf[MAX_BUFF_SIZE])
            FD_SET(STDIN_FILENO,&rset);

        if(recviptr < &recvbuf[MAX_BUFF_SIZE])
            FD_SET(sockfd,&rset);

        if(sendiptr != sendoptr)
            FD_SET(sockfd,&wset);

        if(recviptr != recvoptr)
            FD_SET(STDOUT_FILENO,&wset);

        int ret = select(maxfd,&rset,&wset,NULL,NULL);
        if(ret == 0)continue;
        else if(ret < 0 ) {
            err_sys("select error");
        }
        else
        {
            if(FD_ISSET(STDIN_FILENO,&rset)){
                if((n = read(STDIN_FILENO,sendiptr,&sendbuf[MAX_BUFF_SIZE] - sendiptr)) < 0){
                    if(errno != EWOULDBLOCK)
                        err_sys("read error on stdin");
                }
                else if(n == 0){
                    fprintf(stderr,"%s:EOF on stdin\n",gf_time());
                    stdineof = 1;
                    if(sendiptr == sendoptr)
                        shutdown(sockfd,SHUT_WR);
                }
                else{
                    sendiptr += n;
                    fprintf(stderr,"%s: read %d bytes from stdin\n",gf_time(),n);
                    FD_SET(sockfd,&wset);
                }
            }
            if(FD_ISSET(sockfd,&rset)){
                if((n = read(sockfd,recviptr,&recvbuf[MAX_BUFF_SIZE]-recviptr)) < 0){
                    if(errno != EWOULDBLOCK);
                        err_sys("read error on sockfd");
                }
                else if(n == 0){
                    fprintf(stderr,"%s : EOF on sockfd \n",gf_time());
                    if(stdineof)return ;
                    else err_quit("str_cli:server terminated prematruely");
                }
                else {
                    fprintf(stderr,"%s : read %d bytes from sockfd: %d\n",gf_time(),n,sockfd);
                    recviptr += n;
                    FD_SET(STDOUT_FILENO,&wset);
                }
            }
            if(FD_ISSET(sockfd,&wset) && (n = sendiptr - sendoptr) > 0){
                if((ret = write(sockfd,sendoptr,n)) < 0){
                    if(errno != EWOULDBLOCK)
                        err_sys("write error on sockfd");
                }
                else{
                    fprintf(stderr,"%s : %d bytes write to sockfd: %d\n",gf_time(),ret,sockfd);
                    sendoptr += ret;
                    if(sendiptr == sendoptr) {
                        sendiptr = sendoptr = sendbuf;
                        if(stdineof)
                            shutdown(sockfd,SHUT_WR);
                    }
                }
            }
            if(FD_ISSET(STDOUT_FILENO,&wset) && (n = recviptr - recvoptr) > 0){
                if((ret = write(STDOUT_FILENO,recvoptr,n)) < 0){
                    if(errno != EWOULDBLOCK)
                        err_sys("write error on stdout");
                }
                else
                {
                    fprintf(stderr,"%s : %d bytes write to stdout\n",gf_time(),ret);
                    recvoptr += ret;
                    if(recviptr == recvoptr )
                        recviptr = recvoptr = recvbuf;
                }
            }
        }
    }
}

void str_cli_fork(FILE* fp,int sockfd){
    pid_t  pid;
    char sendbuf[MAX_BUFF_SIZE] = {0};
    char recvbuf[MAX_BUFF_SIZE] = {0};

    if((pid = fork()) == 0){
        while (Readline(sockfd,recvbuf,MAX_BUFF_SIZE) > 0){
            fputs(recvbuf,stdout);
        }
        kill(getppid(),SIGTERM);
        exit(0);
    }
    while(fgets(sendbuf,MAX_BUFF_SIZE,fp) != NULL){
        Writen(sockfd,sendbuf,strlen(sendbuf));
    }
    shutdown(sockfd,SHUT_WR);
    pause();
    return ;
}

int main(int argc,char **argv){
    if(argc != 2){
        err_quit("usage error");
    }

    int connfd;

    connfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
    assert(connfd > 2);

    struct sockaddr_in servaddr;
    bzero(&servaddr,sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(8888);
    inet_pton(AF_INET,argv[1],&servaddr.sin_addr);

    Connect(connfd,(struct sockaddr *)&servaddr,sizeof(servaddr));

    str_cli_fork(stdin,connfd);

    return 0;
}
