//
// Created by Administrator on 2017/2/24.
//

#include "unp.h"

sigFunc Signal(int sig,sigFunc func){
    struct sigaction sac,osac;
    sac.sa_handler = func;
    sigemptyset(&sac.sa_mask);
    sac.sa_flags = 0;
    if(sig == SIGALRM){
#ifdef SA_INTERRUPT
        sac.sa_flags |= SA_INTERRUPT;
#endif
    }
    else{
#ifdef SA_RESTART
        sac.sa_flags |= SA_RESTART;
#endif
    }
    if(sigaction(sig,&sac,&osac) < 0)
        return SIG_ERR;
    return sac.sa_handler;
}

void sig_chld(int signo){
    pid_t  pid;
    int stat;
    while((pid = waitpid(-1,&stat,WNOHANG)) > 0)
        printf("child %d terminated\n",pid);
    return;
}