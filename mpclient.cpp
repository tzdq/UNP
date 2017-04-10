// mpclient2 —— multiple processes client
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
using namespace std;

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>

#define err_exit(msg) cout << "[error] " << msg << endl; exit(1)

struct UserOptions{
    char* server_ip;
    int port;
    int num_clients;
    int sleep_ms;
};

void discovery_user_options(int argc, char** argv, UserOptions& options){
    if(argc <= 4){
        cout << "Usage: " << argv[0] << " <server_ip> <port> <num_clients> <sleep_ms>" << endl
             << "server_ip: the ip address of server" << endl
             << "port: the port to connect at" << endl
             << "num_clients: the num to start client." << endl
             << "sleep_ms: the time to sleep in ms." << endl
             << "for example: " << argv[0] << " 127.0.0.1 1990 1000 500" << endl;
        exit(1);
    }

    options.server_ip = argv[1];
    options.port = atoi(argv[2]);
    options.num_clients = atoi(argv[3]);
    options.sleep_ms = atoi(argv[4]);
    if(options.num_clients <= 0){
        err_exit("num_clients must > 0.");
    }
}

int connect_server_socket(UserOptions& options){
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);

    if(clientfd == -1){
        err_exit("init socket error!");
    }
    cout << "init socket success!" << endl;

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(options.port);
    addr.sin_addr.s_addr = inet_addr(options.server_ip);
    if(connect(clientfd, (const sockaddr*)&addr, sizeof(sockaddr_in)) == -1){
        err_exit("connect socket error!");
    }
    cout << "connect socket success!" << endl;

    return clientfd;
}

int main(int argc, char** argv){
    srand(0);
    UserOptions options;
    discovery_user_options(argc, argv, options);
    int* fds = new int[options.num_clients];
    for(int i = 0; i < options.num_clients; i++){
        fds[i] = connect_server_socket(options);
        usleep(10 * 1000); // to prevent the SIGIO error: maybe signal queue overflow!
    }

    for(int i = 0; i < options.num_clients; i++){
        int fd = fds[i];
        if(true){
            char c = 'C'; // control message
            if(send(fd, &c, sizeof(char), 0) <= 0){
                close(fd);
                err_exit("send message failed!");
            }
            int required_id = rand();
            if(send(fd, &required_id, sizeof(int), 0) <= 0){
                close(fd);
                err_exit("send message failed!");
            }
        }
    }

    bool do_loop = true;
    while(do_loop){
        int ret = 0;
        for(int i = 0; i < options.num_clients; i++){
            int fd = fds[i];
            if(true){
                char c = 'M'; // data message
                ret = send(fd, &c, sizeof(char), 0);
                if(ret <= 0){
                    cout << "send control message to server error" << endl;
                    close(fd);
                    do_loop = false;
                    break;
                }
                char msg[] = "client, ping message!";
                ret = send(fd, msg, sizeof(msg), 0);
                if(ret <= 0){
                    cout << "send control value to server error" << endl;
                    close(fd);
                    do_loop = false;
                    break;
                }
                memset(&msg, 0, sizeof(msg));
            }
            if(true){
                char buf[1024];
                memset(buf, 0, sizeof(buf));
                ret = recv(fd, buf, sizeof(buf), 0);
                if(ret <= 0){
                    cout << "recv from server error" << endl;
                    close(fd);
                    do_loop = false;
                    break;
                }
                cout << "recv from server: " << buf << endl;
            }

            usleep(options.sleep_ms * 1000);
        }
    }

    delete[] fds;

    return 0;
}
