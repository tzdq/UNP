//
// Created by Administrator on 2017/3/23.
//
#include "unp.h"
#include <iostream>

using namespace std;

int main(int argc ,char **argv){
    hostent *hostptr;
    struct in_addr inetaddr;
    struct in_addr *inetaddrp[2];
    struct in_addr **paddr;
    struct servent  *sp;
    char str[MAX_BUFF_SIZE] = {0};
    int sockfd;
    struct sockaddr_in addr;
    socklen_t  len;
    //hostname = dev

    if(argc != 3)
        err_quit("usage:daytimetcpclient <hostname> <service>");

    if((hostptr = gethostbyname(argv[1]) ) == NULL){
        if(inet_aton(argv[1],&inetaddr) == 0)
            err_sys("inet_aton error for %s:%s",argv[1],hstrerror(h_errno));
        else{
            inetaddrp[0] = &inetaddr;
            inetaddrp[1] = NULL;
            paddr = inetaddrp;
        }
    }
    else
    {
        paddr = (struct in_addr **)hostptr->h_addr_list;
        for(;*paddr != NULL;++paddr){
            cout << " ip " << inet_ntop(AF_INET,*paddr,str, sizeof(struct sockaddr_in)) << endl;
        }
    }

    if((sp = getservbyname(argv[2],"tcp") ) == NULL)
        err_sys("getservbyname error");
    else{
        cout << "name = " << sp->s_name << endl;
        cout << "port = "<< sp->s_port <<endl;
        cout <<" proto = " << sp->s_proto << endl;
    }

    for(;*paddr != NULL;++paddr){
        sockfd = socket(AF_INET,SOCK_STREAM,0);
        if(sockfd == -1)
            err_sys("socket error");
        bzero(&addr,sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = sp->s_port;
        memcpy(&addr.sin_addr,*paddr, sizeof(struct in_addr));
        cout << "trying to " << inet_ntop(AF_INET,*paddr,str, sizeof(struct sockaddr_in)) << endl;

        if(connect(sockfd,(struct sockaddr*)&addr,sizeof(addr)) == 0)
            break;
        err_ret("connect error");
        close(sockfd);
    }
    if(*paddr == NULL)
        err_quit("unable to connect");

    int n = 0;
    while((n = read(sockfd,str,MAX_BUFF_SIZE)) > 0){
        str[n] = '\0';
        fputs(str,stdout);
    }

    return 0;
}
