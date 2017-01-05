//
// Created by Administrator on 2017/1/4.
//

#include "utils.h"

char *sock_ntop(const struct sockaddr *addr, socklen_t len) {
    char portstr[8] = {0};
    static char str[128];

    memset(portstr,0, sizeof(portstr));

    switch (addr->sa_family){
        case AF_INET:
            struct sockaddr_in *sin = (struct sockaddr_in*)addr;
            if(inet_ntop(AF_INET,&sin->sin_addr,str, sizeof(str)) == NULL){
                return nullptr;
            }
            if(ntohs(sin->sin_port) != 0){
                snprintf(portstr, sizeof(portstr),":%d",ntohs(sin->sin_port));
                strcat(str,portstr);
            }
            break;
    }
    return str;
}
/**
 * @param family
 * @param strptr
 * @param addrptr
 * @return success(1) fail(0)
 */
int inet_pton_loose(int family,const char* strptr,void *addrptr) {
    if(inet_pton(family,strptr,addrptr) == 0){
        struct in_addr addr;
        if(inet_aton(strptr,&addr)){
            if(family == AF_INET){
                memcpy(addrptr,&addr,sizeof(addr));
                return 1;
            }
            else if(family == AF_INET6){
                struct in6_addr addrv6;
                for(auto i = 0 ; i < 16 ; ++i){
                    if(i < 10)addrv6.__u6.__s6_addr[i]= 0;
                    else if(i < 12)addrv6.__u6.__s6_addr[i] = 0xFF;
                    else {
                        addrv6.__u6.__s6_addr[i] = (htonl(addr.s_addr) >> ((15-i)*8) & 0xFF);

                    }
                }
                memcpy(addrptr,(void *)&addrv6,sizeof(addrv6));
                return 1;
            }
            return 1;
        }
        return 0;
    }
    return 1;
}
