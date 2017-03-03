//
// Created by Administrator on 2017/1/6.
//
#include "unp.h"

static int read_cnt;
static char *read_ptr;
static char read_buf[MAX_BUFF_SIZE];
/**
 * @func  一次读取n个字节的数据
 * @param fd   文件描述符
 * @param ptr  数据缓冲区
 * @param n    n个字节
 * @return     成功返回剩余未读的字节数，失败返回-1
 */
ssize_t  readn(int fd,void *ptr,size_t n){
    size_t  nLeft = n;
    size_t  nRead = 0;
    char* strptr ;
    strptr = (char*)ptr;
    while (nLeft > 0){
        if((nRead = read(fd,strptr,nLeft)) < 0 ){
            if(errno == EINTR)nRead = 0;
            else return -1;
        }
        else if(nRead == 0){
            break;
        }
        nLeft -= nRead;
        strptr += nRead;
    }

    return n- nLeft;
}
/**
 * @func  向文件描述符fd写入n个字节的数据
 * @param fd
 * @param ptr 写入的数据
 * @param n
 * @return 成功返回n，失败返回-1
 */
ssize_t writen(int fd,const void *ptr,size_t n){
    size_t nLeft = n;
    size_t nWrite = 0;
    const char *strptr;
    strptr = (const char *)ptr;
    while (nLeft > 0){
        if((nWrite = write(fd,strptr,nLeft)) <= 0){
            if(nWrite < 0 && errno == EINTR)nWrite = 0;
            else return -1;
        }
        nLeft -= nWrite;
        strptr += nWrite;
    }
    return n;
}

void Writen(int fd, void *ptr, size_t nbytes)
{
    if (writen(fd, ptr, nbytes) != nbytes)
        err_sys("writen error");
}


static ssize_t my_read(int fd,char *ptr){
    if(read_cnt <= 0){
        again:
        if((read_cnt = read(fd,read_buf,MAX_BUFF_SIZE)) < 0){
            if(errno == EINTR)goto again;
            return -1;
        }
        else if(read_cnt == 0)
            return 0;
        read_ptr = read_buf;
    }
    read_cnt--;
    *ptr = *read_ptr++;
    return 1;
}

ssize_t  readline(int fd,void *vptr,size_t maxlen){
    ssize_t n,rc;
    char c,*ptr;
    ptr = (char*)vptr;
    for(n = 1;n < maxlen;n++){
        if((rc = my_read(fd,&c)) == 1){
            *ptr++ = c;
            if(c == '\n')
                break;
        }
        else if(rc == 0){
            *ptr = 0;
            return (n-1);
        }
        else return -1;
    }
    *ptr = 0;
    return n;
}
ssize_t Readline(int fd, void *ptr, size_t maxlen)
{
    ssize_t		n;

    if ( (n = readline(fd, ptr, maxlen)) < 0)
        err_sys("readline error");
    return(n);
}