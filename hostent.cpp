//
// Created by Administrator on 2017/3/22.
//

#include "unp.h"
#include <netdb.h>

int main(int argc,char **argv){
    char *ptr,**pptr;
    char str[MAX_BUFF_SIZE] = {0};
    struct hostent *hptr;

    while(--argc > 0){
        ptr = *++argv;
        if((hptr = gethostbyname(ptr)) == NULL){
            err_msg("gethostbyname error for host:%s:%s",ptr,hstrerror(h_errno));
            continue;
        }
        std::cout <<  "officaial hostname " << hptr->h_name << std::endl;

        for (pptr  = hptr->h_aliases; pptr != NULL ; ++pptr) {
            std::cout << "\talias:"<<*pptr<<std::endl;
        }

        switch (hptr->h_addrtype){
            case AF_INET:
                pptr = hptr->h_addr_list;
                for(;pptr != NULL;++pptr){
                    std::cout << "\taddress:"<<inet_ntop(AF_INET,*pptr,str,sizeof(str))<<std::endl;
                }
                break;
            default:
                err_ret("unknown address type");
                break;
        }
    }

    return 0;
}