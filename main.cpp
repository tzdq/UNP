#include <iostream>
#include <netinet/in.h>
#include <sys/types.h>
#include "utils.h"
using namespace std;
int main() {
    union {
        short s;
        char c[sizeof(s)];
    }un;

    un.s = 0x0102;
    if(un.c[0] == 1 && un.c[1] == 2)cout << "Big"<<endl;
    else if(un.c[0] == 2 && un.c[1] == 1)cout <<"little" <<endl;
    else cout << "unknown"<<endl;

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

    return 0;
}