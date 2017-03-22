//
// Created by Administrator on 2017/3/6.
//

#ifndef UNP_WEB_H
#define UNP_WEB_H

#include "unp.h"

#define MAX_FILE_NUM 20
#define SERV_PORT    "80"

struct File{
    char *name;
    char *host;
    int   fd;
    int   flags; //标志
}g_file[MAX_FILE_NUM];

#define F_CONNECTING 1
#define F_READING    2
#define F_DONE        4

#define  GET_CMD "GET %s HTTP/1.1\r\n\r\n"

int g_nConn,g_nFiles,g_nLeftToConn,g_nLeftToRead,g_maxFd;
fd_set g_rSet,g_wSet;

void home_page(const char *host,const char *fname);
void start_connect(struct File *fptr);
void write_get_cmd(struct File *fptr);
#endif //UNP_WEB_H
