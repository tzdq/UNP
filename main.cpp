#include <iostream>
#include <netinet/in.h>
#include <sys/types.h>
#include "utils.h"
#include <vector>
#include <fstream>
#include "unp.h"
using namespace std;
int main() {
#if 0
     uint16_t port =  htons(1234);
    cout << port <<endl;

    unsigned char *strptr;
    struct in_addr addr;
    struct in6_addr addrv6;

    if(inet_pton_loose(AF_INET,"192.168.1.54",(void*)&addr)){
        strptr = (unsigned char*)&addr;
        printf("%d.%d.%d.%d\n",strptr[0],strptr[1],strptr[2],strptr[3]);
    }
    if(inet_pton_loose(AF_INET6,"192.168.1.54",(void*)&addrv6)){
        strptr = (unsigned char*)&addrv6;
        printf("%d%d:%d%d:%d%d:%d%d:%d%d:%x%x:%d.%d.%d.%d\n",strptr[0],strptr[1],
               strptr[2],strptr[3],strptr[4],strptr[5],strptr[6],strptr[7],strptr[8],strptr[9]
                ,strptr[10],strptr[11],strptr[12],strptr[13],strptr[14],strptr[15]);
    }

    struct  sockaddr_in6 saddr;
    memset(&saddr,0, sizeof(saddr));
    saddr.sin6_family = AF_INET6;
    saddr.sin6_port = htons(8888);
    char str[128] = {0};
    inet_pton(AF_INET6,"::FFFF:192.168.1.54",(void*)&saddr.sin6_addr);
    cout << sock_ntop((struct sockaddr*)&saddr, sizeof(saddr)) <<endl;
    cout << inet_ntop(AF_INET6,&saddr.sin6_addr,str,sizeof(str))<<endl;

    vector<int> v= {1,2};
    auto i = v.front();
    i = 10;
    cout << v.front();

    cout << judgeEnduan()<<endl;
    struct sockaddr_in addr;
    char buf[INET_ADDRSTRLEN] = {0};
    cout << my_inet_pton_ipv4(AF_INET,"192.168.1.54",&addr)<<endl;
    cout << my_inet_ntop_ipv4(AF_INET,&addr,buf,INET_ADDRSTRLEN)<<endl;
#else
    fstream fin("../a.txt");
    for(int i = 0 ; i < 2000;i++){
        fin << i << endl;
    }

#endif
    return 0;
}