//
// Created by Administrator on 2017/2/17.
//
#include <syslog.h>
#include <stdarg.h>
#include "unp.h"


int daemon_proc;
static void err_doit(int errnoflag,int level,const char* fmt,va_list ap);

void err_ret(const char *fmt,...){
    va_list ap;
    va_start(ap,fmt);
    err_doit(1,LOG_INFO,fmt,ap);
    va_end(ap);
    return ;
}
void err_sys(const char *fmt,...){
    va_list ap;
    va_start(ap,fmt);
    err_doit(1,LOG_ERR,fmt,ap);
    va_end(ap);
    exit(1);
}

void err_dump(const char *fmt,...){
    va_list ap;
    va_start(ap,fmt);
    err_doit(1,LOG_ERR,fmt,ap);
    va_end(ap);
    abort();
    exit(1);
}

void err_msg(const char *fmt,...){
    va_list ap;
    va_start(ap,fmt);
    err_doit(0,LOG_INFO,fmt,ap);
    va_end(ap);
    return;
}

void err_quit(const char *fmt,...){
    va_list ap;
    va_start(ap,fmt);
    err_doit(0,LOG_ERR,fmt,ap);
    va_end(ap);
    exit(1);
}

static void err_doit(int errnoflag,int level,const char* fmt,va_list ap){
    int error_sava,n;
    char buf[MAXLINE + 1] = {0};
    error_sava = errno;
#ifdef HAVA_VSNPRINTF
    vsnprintf(buf,MAXLINE,fmt,ap);
#else
    vsnprintf(buf,fmt,ap);
#endif

    n = strlen(buf);
    if(errnoflag)
        snprintf(buf+n,MAXLINE - n,": %s",strerror(error_sava));
    strcat(buf,"\n");

    if(daemon_proc)
        syslog(level,buf);
    else{
        fflush(stdout);
        fputs(buf,stderr);
        fflush(stderr);
    }
    return ;
}