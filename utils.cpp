//
// Created by Administrator on 2017/1/4.
//

#include "utils.h"

/**
 * @func  实现IPV4和IPV6的ntop操作，不用指定具体的协议，这儿主要针对AF_INET和AF_INET6
 * @param addr  IP地址信息
 * @param len   IP地址长度
 * @return inet_ntop失败返回nullptr，协议错误返回带错误信息的字符串strptr，反之返回点分十进制的IP地址
 */
char *sock_ntop(const struct sockaddr *addr, socklen_t len) {
    char portstr[8];
    static char strptr[128];

    memset(portstr,0, sizeof(portstr));
    memset(strptr,0, sizeof(strptr));

    switch (addr->sa_family){
        case AF_INET:{
            struct sockaddr_in *sin = (struct sockaddr_in*)addr;
            if(inet_ntop(AF_INET,&sin->sin_addr,strptr, sizeof(strptr)) == nullptr){
                return nullptr;
            }
            if(ntohs(sin->sin_port) != 0){
                snprintf(portstr, sizeof(portstr),":%d",ntohs(sin->sin_port));
                strcat(strptr,portstr);
            }
            return strptr;
        }
#ifdef AF_INET6
        case AF_INET6: {
            struct sockaddr_in6 *sinv6 = (struct sockaddr_in6 *) addr;
            strptr[0] = '[';
            if (inet_ntop(AF_INET6, &sinv6->sin6_addr, strptr + 1, sizeof(strptr) - 1) == nullptr)
                return nullptr;
            if (ntohs(sinv6->sin6_port) != 0) {
                snprintf(portstr, sizeof(portstr), "]:%d", ntohs(sinv6->sin6_port));
                strcat(strptr, portstr);
                return strptr;
            }
            return (strptr + 1);//ntohs失败，不输出strptr[0]
        }
#endif
        default:
            snprintf(strptr, sizeof(strptr), "sock_ntop: unknown AF_xxx: %d, len %d", addr->sa_family, len);
            return strptr;
    }
    return nullptr;
}
/**
 * UNP 第三章第3题
 * @param family 协议
 * @param strptr IP地址字符串
 * @param addrptr  转换后的IP，IPV4映射的IPV6地址的格式[::FFFF:xxx.xxx.xxx.xxx]
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
