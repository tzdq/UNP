//
// Created by Administrator on 2017/1/6.
//
#include "unp.h"
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