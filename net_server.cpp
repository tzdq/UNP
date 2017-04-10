#include<sys/types.h>
#include<sys/socket.h>
#include<stdio.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<unistd.h>
#include<string.h>
#include<stdlib.h>
#include<fcntl.h>
#include<sys/shm.h>
#include<signal.h>
#include<errno.h>
#include <iostream>
using  namespace std;

#define MYPORT 9877   //服务器端口
#define QUEUE 20        //等待队列大小
#define BUFFER_SIZE 1024 //缓冲区大小

int main()
{
    //定义socket
    int server_sockfd = socket(AF_INET,SOCK_STREAM,0);
    //定义sockaddr_in
    struct sockaddr_in server_sockaddr;
    server_sockaddr.sin_family = AF_INET;
    server_sockaddr.sin_port = htons(MYPORT);
    server_sockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    //bind，成功返回0,出错返回-1
    if(bind(server_sockfd,(struct sockaddr *)&server_sockaddr,sizeof(server_sockaddr))==-1)
    {
        perror("bind error!\n");
        exit(1);
    }
    //lisen,成功返回0,出错返回-1
    if(listen(server_sockfd,QUEUE)==-1)
    {
        perror("lisen error!\n");
        exit(1);
    }
    int count=0;
    while(1)
    {
        //客户端套接字
        char buffer[BUFFER_SIZE];
        struct sockaddr_in client_addr;
        socklen_t length = sizeof(client_addr);
        //接收连接 成功返回非负描述字，出错返回-1
        int conn = accept(server_sockfd,(struct sockaddr*)&client_addr,&length);
        if(conn<0)
        {
            if(errno==EINTR)
            {
                continue;
            }
            perror("connect error!\n");
            exit(1);
        }
        cout << "connfd = " << conn << endl;
        int pid;
        //调用fork()函数时  如果是父进程调用则返回进程号，如果是子进程调用则返回0
        //这里父进程用来创建连接  子进程负责处理请求  所以当pid=0才执行
        if((pid=fork())==0)
        {
            //子进程会复制父进程的资源 这里我们关闭连接只是把子进程复制的连接关闭并不会真正关闭连接
            close(server_sockfd);
            while(1)
            {
                //清空缓冲区
                memset(buffer,0,sizeof(buffer));
                //接收客户端发送来的数据
                int len = recv(conn,buffer,sizeof(buffer),0);
                if(strcmp(buffer,"exit\n")==0||len==0)
                {
                    break;
                }
                fputs(buffer,stdout);
                //把相同的数据发送给客户端
                send(conn,buffer,len,0);
            }
            close(conn);
            exit(0);
        }
        close(conn);
        count++;
    }
    close(server_sockfd);
    return 0;
}//
// Created by Administrator on 2017/3/31.
//

