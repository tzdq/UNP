// mpserver2 —— multiple processes server
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <iostream>
#include <vector>
using namespace std;

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/wait.h>
#include <sys/epoll.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#define info(msg) cout << "#" << getpid() << " " << msg
#define err_exit(msg) info("[error] " << msg << ", errno=0x" << hex << errno << dec << " " << strerror(errno) << endl); exit(-1)

// define the SIGIO to RTSig, the SIGIO now means rt-sig queue full.
#define SIG_SOCKET_IO SIGRTMIN+1

bool global_terminate = false;
bool global_channel_msg = false;
// when global_channel_msg is true, the current_active_io_fd set to the fd.
int current_active_io_fd = -1;

int current_client_num = 0;
bool accepting = true;

// the num of client for a proecess to serve.
// 2+N
// 2: the serverfd, the channelfd
// N: clients.
#define max_clients_per_process 2+1000
// the physical max clients, if exceed this value, error!
#define physical_max_clients 1024

#define max_workers 100

struct UserOptions{
    int port;
    int num_processes;
};

void discovery_user_options(int argc, char** argv, UserOptions& options){
    if(argc <= 2){
        cout << "Usage: " << argv[0] << " <port> <num_processes>" << endl
            << "port: the port to listen" << endl
            << "num_processes: the num to start processes. if 0, use single process mode" << endl
            << "for example: " << argv[0] << " 1990 20" << endl;
        exit(1);
    }

    options.port = atoi(argv[1]);
    options.num_processes = atoi(argv[2]);
}

#define CMD_FD 100
#define CMD_INFO 200
struct channel_msg{
    int command;
    int fd; // the fd, set to -1 if no fd.
    int total_client; // the total clients number of specified process.
    int param; // the param.
};
void write_channel(int sock, channel_msg* data, int size)
{
    msghdr msg;

    // init msg_control
    if(data->fd == -1){
        msg.msg_control = NULL;
        msg.msg_controllen = 0;
    }
    else{
        union {
            struct cmsghdr cm;
            char space[CMSG_SPACE(sizeof(int))];
        } cmsg;
        memset(&cmsg, 0, sizeof(cmsg));

        cmsg.cm.cmsg_level = SOL_SOCKET;
        cmsg.cm.cmsg_type = SCM_RIGHTS; // we are sending fd.
        cmsg.cm.cmsg_len = CMSG_LEN(sizeof(int));

        msg.msg_control = (cmsghdr*)&cmsg;
        msg.msg_controllen = sizeof(cmsg);
        *(int *)CMSG_DATA(&cmsg.cm) = data->fd;
    }

    // init msg_iov
    iovec iov[1];
    iov[0].iov_base = data;
    iov[0].iov_len  = size;

    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    // init msg_name
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    info("[write_channel][info] data fd=" << data->fd << ", command="
        << data->command << ", total_client=" << data->total_client
        << ", param=" << data->param << endl);

    while(true){
        ssize_t ret;
        if ((ret = sendmsg(sock, &msg, 0)) <= 0){
            // errno = 0xb Resource temporarily unavailable
            // donot retry, for it's no use, error infinite loop.
            err_exit("[write_channel] sendmsg error, ret=" << ret);
        }
        break;
    }
}
void read_channel(int sock, channel_msg* data, int size)
{
    msghdr msg;

    // msg_iov
    iovec iov[1];
    iov[0].iov_base = data;
    iov[0].iov_len = size;

    msg.msg_iov = iov;
    msg.msg_iovlen  = 1;

    // msg_name
    msg.msg_name = NULL;
    msg.msg_namelen = 0;

    // msg_control
    union { // union to create a 8B aligned memory.
        struct cmsghdr cm; // 16B = 8+4+4
        char space[CMSG_SPACE(sizeof(int))]; // 24B = 16+4+4
    } cmsg;
    memset(&cmsg, 0, sizeof(cmsg));

    msg.msg_control = (cmsghdr*)&cmsg;
    msg.msg_controllen = sizeof(cmsg);

    ssize_t ret;
    if ((ret = recvmsg(sock, &msg, 0)) == -1) {
        err_exit("[read_channel] recvmsg error");
    }
    if(ret == 0){
        info("[read_channel] connection closed" << endl);
        exit(0);
    }

    info("[read_channel][info] data fd=" << data->fd << ", command="
        << data->command << ", total_client=" << data->total_client
        << ", param=" << data->param << endl);
    if(data->command == CMD_FD){
        int fd = *(int *)CMSG_DATA(&cmsg.cm);
        if(fd == 0 || fd == 1){
            err_exit("[read_channel] recvmsg invalid fd=" << fd
                << ", data->fd=" << data->fd << ", command=" << data->command
                << ", ret=" << ret);
        }
        data->fd = fd;
    }
    else{
        data->fd = -1;
    }
}

void set_noblocking(int fd){
    int flag = 1;
    if(ioctl(fd, FIONBIO, &flag) == -1){
        err_exit("[master] ioctl failed!");
    }
    info("[master] server socket no-block flag set success" << endl);
}

int listen_server_socket(UserOptions& options){
    int serverfd = socket(AF_INET, SOCK_STREAM, 0);

    if(serverfd == -1){
        err_exit("[master] init socket error!");
    }
    info("[master] init socket success! #" << serverfd << endl);

    int reuse_socket = 1;
    if(setsockopt(serverfd, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(int)) == -1){
        err_exit("[master] setsockopt reuse-addr error!");
    }
    info("[master] setsockopt reuse-addr success!" << endl);

    set_noblocking(serverfd);

    sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(options.port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if(bind(serverfd, (const sockaddr*)&addr, sizeof(sockaddr_in)) == -1){
        err_exit("[master] bind socket error!");
    }
    info("[master] bind socket success!" << endl);

    if(listen(serverfd, 10) == -1){
        err_exit("[master] listen socket error!");
    }
    info("[master] listen socket success! " << options.port << endl);

    return serverfd;
}

// both master and worker will invoke this handler.
void signal_handler(int signo, siginfo_t* info, void* context){
    info("#" << getpid() << " get a signal:");

    if(signo == SIG_SOCKET_IO){
        cout << "SIG_SOCKET_IO!" << endl;
        global_channel_msg = true;
        current_active_io_fd = info->si_fd;
        info("[master] get SIGIO: fd=" << info->si_fd << endl);
        return;
    }

    switch(signo){
        case SIGCHLD:
            cout << "SIGCHLD";
            break;
        case SIGALRM:
            cout << "SIGALRM";
            break;
        case SIGIO:
            cout << "SIGIO";
            info("[master][warning] ignore the SIGIO: rt-sig queue is full!" << endl);
            break;
        case SIGINT:
            cout << "SIGINT";
            global_terminate = true;
            break;
        case SIGHUP:
            cout << "SIGHUP";
            global_terminate = true;
            break;
        case SIGTERM:
            cout << "SIGTERM";
            global_terminate = true;
            break;
        case SIGQUIT:
            cout << "SIGQUIT";
            global_terminate = true;
            break;
        case SIGUSR1:
            cout << "SIGUSR1";
            break;
        case SIGUSR2:
            cout << "SIGUSR2";
            break;
    }

    cout << "!" << endl;
}

void register_signal_handler_imp(int signum, void (*handler)(int, siginfo_t*, void*)){
    struct sigaction action;

    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handler;
    sigemptyset(&action.sa_mask);
    //action.sa_flags = 0;

    if(sigaction(signum, &action, NULL) == -1){
        err_exit("[master] register signal handler failed!");
    }
}
void register_signal_handler(){
    register_signal_handler_imp(SIGCHLD, signal_handler);
    register_signal_handler_imp(SIGALRM, signal_handler);
    register_signal_handler_imp(SIGIO, signal_handler);
    register_signal_handler_imp(SIG_SOCKET_IO, signal_handler);
    register_signal_handler_imp(SIGINT, signal_handler);
    register_signal_handler_imp(SIGHUP, signal_handler);
    register_signal_handler_imp(SIGTERM, signal_handler);
    register_signal_handler_imp(SIGQUIT, signal_handler);
    register_signal_handler_imp(SIGUSR1, signal_handler);
    register_signal_handler_imp(SIGUSR2, signal_handler);
    info("[master] register signal handler success!" << endl);
}

void block_specified_signals(){
    sigset_t set;

    sigemptyset(&set);

    sigaddset(&set, SIGCHLD);
    sigaddset(&set, SIGALRM);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIG_SOCKET_IO);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGHUP);
    sigaddset(&set, SIGTERM);
    sigaddset(&set, SIGQUIT);
    sigaddset(&set, SIGUSR1);
    sigaddset(&set, SIGUSR2);

    if(sigprocmask(SIG_BLOCK, &set, NULL) == -1){
        err_exit("[master] sigprocmask block signal failed!");
    }
    info("[master] sigprocmask block signal success!" << endl);
}

void unblock_all_signals(){
    sigset_t set;
    sigemptyset(&set);

    // the master process block all signals, we unblock all for we use epoll to wait events.
    if(sigprocmask(SIG_SETMASK, &set, NULL) == -1){
        err_exit("[worker] sigprocmask unblock signal failed!");
    }
    info("[worker] sigprocmask unblock signal success!" << endl);
}

void epoll_add_event(int ep, int fd){
    epoll_event ee;
    ee.events = EPOLLIN;
    ee.data.fd = fd;
    if(epoll_ctl(ep, EPOLL_CTL_ADD, fd, &ee) == -1){
        err_exit("[worker] epoll add event failed!");
    }
    current_client_num ++;
    info("[worker] epoll add fd success: #" << fd << ", current_client_num=" << current_client_num << endl);
}
void epoll_remove_event(int ep, int fd){
    if(epoll_ctl(ep, EPOLL_CTL_DEL, fd, NULL) == -1){
        err_exit("[worker] epoll remove event failed! fd=" << fd << ", errno=0x" << hex << errno << dec << " " << strerror(errno));
    }
    current_client_num --;
    info("[worker] epoll remove fd success: #" << fd << ", current_client_num=" << current_client_num << endl);
}

void close_client(int ep, int fd, int serverfd, int worker_channel, bool do_report = true){
    info("[worker] the client dead, remove it: #" << fd << endl);
    epoll_remove_event(ep, fd);
    close(fd);

    if(serverfd == fd){
        info("[worker] warning to close the server fd" << endl);
    }

    // start accept again.
    if(!accepting && current_client_num < max_clients_per_process){
        epoll_add_event(ep, serverfd);
        accepting = true;
    }

    // notice the master: worker can take more.
    if(do_report){
        channel_msg msg;
        msg.command = CMD_INFO;
        msg.total_client = current_client_num;
        msg.fd = -1;
        msg.param = 0;
        write_channel(worker_channel, &msg, sizeof(channel_msg));
    }
}

void worker_get_event(int ep, int serverfd, epoll_event& active, int worker_channel){
    info("[worker] process #" << getpid() << " current_client_num=" << current_client_num << ", worker_channel=" << worker_channel << endl);
    int fd = active.data.fd;

    // server listening socket event.
    if(fd == serverfd){
        if(current_client_num < physical_max_clients){
            int client = accept(serverfd, NULL, 0);
            if(client == -1){
                // thundering herd
                // http://www.citi.umich.edu/projects/linux-scalability/reports/accept.html
                if(errno == EAGAIN || errno == EWOULDBLOCK){
                    info("[worker][warning] get a thundering herd at #" << getpid() << endl);
                    return;
                }
                err_exit("[worker] accept client socket error!");
            }

            int reuse_socket = 1;
            if(setsockopt(client, SOL_SOCKET, SO_REUSEADDR, &reuse_socket, sizeof(int)) == -1){
                err_exit("[worker] setsockopt reuse-addr error!");
            }
            info("[worker] setsockopt reuse-addr success!" << endl);

            info("[worker] process #" << getpid() << " get a client: #" << client << endl);
            epoll_add_event(ep, client);
        }
        else{
            epoll_remove_event(ep, fd); // donot accept.
            accepting = false;
            info("[worker] warning, the worker #" << getpid()
                << " exceed the max clients, current_client_num=" << current_client_num << endl);
        }
        return;
    }

    // channel event.
    if(fd == worker_channel){
        channel_msg msg;
        read_channel(worker_channel, &msg, sizeof(channel_msg));
        info("[worker] get a channel message. fd=" << msg.fd << ", command=" << msg.command
            << ", current_client_num=" << current_client_num << ", max_clients_per_process=" << max_clients_per_process
            << ", physical_max_clients=" << physical_max_clients << endl);
        if(msg.command == CMD_FD && msg.fd != -1){
            if(msg.fd == 0 || msg.fd == 1){
                info("[worker][warning] ignore the invalid passing fd=" << msg.fd << endl);
                return;
            }
            // accept all client from master, util the read overloaded.
            if(current_client_num < physical_max_clients){
                epoll_add_event(ep, msg.fd);
                info("[worker] get a dispatched client. #" << msg.fd << endl);
            }
            else{
                info("[worker][warning] worker overloaded, close the fd: " << msg.fd << endl);
                close(msg.fd);
            }
        }
        else{
            err_exit("[worker] get a invalid channel");
        }
        return;
    }

    // client event.
    if((active.events & EPOLLHUP) == EPOLLHUP || (active.events & EPOLLERR) == EPOLLERR){
        info("[worker] get a EPOLLHUP or EPOLLERR event from client #" << fd << endl);
        close_client(ep, fd, serverfd, worker_channel);
        return;
    }
    if((active.events & EPOLLIN) == EPOLLIN){
        if(true){
            info("[worker] get a EPOLLIN event from client #" << fd << endl);
            char ch_control; // we MUST not read more bytes, for the fd maybe passing to other process!
            if(recv(fd, &ch_control, sizeof(char), 0) <= 0){
                close_client(ep, fd, serverfd, worker_channel);
                return;
            }
            // check the first byte, if 'M' it's message, if 'C' it's control message.
            if(ch_control == 'C'){
                int client_required_id; //worker_channel
                if(recv(fd, &client_required_id, sizeof(int), 0) <= 0){
                    close_client(ep, fd, serverfd, worker_channel);
                    return;
                }

                pid_t pid = getpid();
                info("[worker] get client control message: client_required_id=" << client_required_id << endl);
                if((client_required_id % pid) == 0){
                    info("[worker][verified] client exactly required this server" << endl);
                }
                else{
                    // never send the follows:
                    if(fd == 0 || fd == 1 || fd == serverfd || fd == worker_channel){
                        info("[worker][warning] ignore invalid fd to passing: " << fd << endl);
                        close_client(ep, fd, serverfd, worker_channel);
                        return;
                    }
                    info("[worker] send to master to dispatch it" << endl);
                    channel_msg msg;
                    msg.fd = fd;
                    msg.command = CMD_FD;
                    msg.total_client = current_client_num;
                    msg.param = client_required_id;
                    write_channel(worker_channel, &msg, sizeof(channel_msg));
                    // donot report to master, for the CMD_FD message has reported.
                    close_client(ep, fd, serverfd, worker_channel, false);
                    return;
                }
            }
            else{
                char buf[1024];
                 if(recv(fd, buf, sizeof(buf), 0) <= 0){
                    close_client(ep, fd, serverfd, worker_channel);
                    return;
                }
               info("[worker] get client data message: " << buf << endl);
            }
        }
        if(true){
            char msg[] = "hello, server ping!";
            if(send(fd, msg, sizeof(msg), 0) <= 0){
                close_client(ep, fd, serverfd, worker_channel);
                return;
            }
        }
        return;
    }
}

void worker_process_cycle(int serverfd, int worker_channel){
    cout << "[worker] start worker process cycle. serverfd=" << serverfd
        << ", worker_channel=" << worker_channel
        << ((worker_channel == -1)? "(single process)" : "(multiple processes)")<< endl;
    unblock_all_signals();

    int ep = epoll_create(1024);
    if(ep == -1){
        err_exit("[worker] epoll_create failed!");
    }
    info("[worker] epoll create success!" << endl);
    epoll_add_event(ep, serverfd);
    epoll_add_event(ep, worker_channel);

    // worker use epoll event, the signal handler will break the epoll_wait to return -1.
    for(;;){
        epoll_event events[1024];
        int incoming = epoll_wait(ep, events, 1024, -1);

        // get a event or error.
        if(incoming == -1){
            if(global_terminate){
                close(worker_channel);
                close(serverfd);
                info("[worker] worker process exit" << endl);
                exit(0);
            }
            break;
        }

        for(int i = 0; i < incoming; i++){
            worker_get_event(ep, serverfd, events[i], worker_channel);
        }
    }

    info("[worker] worker process exited" << endl);
    close(ep);
}

struct WorkerProcess{
    pid_t pid;
    int channel;
    int num_clients;
};

void start_worker_process(int serverfd, WorkerProcess& worker_process){
    // master/worker use domain socket to communicate.
    int fds[2];
    if(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == -1){
        err_exit("[master] sockpair create domain socket failed");
    }
    info("[master] sockpair create domain socket success! [" << fds[0] << ", " << fds[1] << "]" << endl);

    // master process, use the fds[0].
    worker_process.num_clients = 0;
    worker_process.channel = fds[0];

    /**
    * SIGIO (http://www.lindevdoc.org/wiki/SIGIO)
    * The SIGIO signal is sent to a process to notify it about some event on a file descriptor, namely:
    *   the file descriptor is ready to receive data (previous write has finished).
    *   the file descriptor has new data to read
    *   an error has occurred
    * This signal is not sent by default; it must be enabled by specifying the O_ASYNC flag to the descriptor
    *
    * O_ASYNC (http://www.lindevdoc.org/wiki/O_ASYNC)
    * The O_ASYNC flag specifies that the process that owns a data stream (by default the process that opened it,
    * but can be changed with fcntl() with the F_SETOWN command) will receive a signal (SIGIO by default, but can
    * be changed with fcntl() with the F_SETSIG command) when the file is ready for reading or writing.
    */
    // set the socket owner to master, to get a SIGIO signal.
    if(fcntl(worker_process.channel, F_SETOWN, getpid()) == -1){
        err_exit("[master] fcntl F_SETOWN to master failed");
    }
    info("[master] fcntl F_SETOWN to master success" << endl);
    // set socket to async to singal SIGIO when data coming.
    int on = 1;
    if(ioctl(worker_process.channel, FIOASYNC, &on) == -1){
        err_exit("[master] ioctl set FIOASYNC failed");
    }
    info("[master] ioctl set FIOASYNC success" << endl);
    /** F_SETSIG
    * if donot use set_sig, we must use select to find which fd is read.
    * http://ajaxxx.livejournal.com/62378.html
    * Linux has the additional feature of fcntl(F_SETSIG), which lets you get the file descriptor that generated the
    * signal back as part of the signal information. So I implemented this, with the obvious semantics: receiving a
    * SIGIO would call the handler for just that file descriptor and then return.
    * remark: the SIGRTMIN=34, SIGRTMAX=62, so only 28 fd cannbe discoveried. so it's used for category signal,
    *       and we use SA_SIGINFO for sigaction to get the fd of SIGIO.
    */
    if(fcntl(worker_process.channel, F_SETSIG, SIG_SOCKET_IO) == -1){
        err_exit("[master] fcntl set rtsig failed: F_SETSIG.");
    }
    info("[master] fcntl set rtsig success" << endl);
    // set to unblocking.
    set_noblocking(worker_process.channel);
    set_noblocking(fds[1]);

    // start process by fork.
    pid_t pid = fork();

    if(pid == -1){
        err_exit("[master] fork process failed");
    }

    // worker process, use the fds[1].
    if(pid == 0){
        close(fds[0]);
        worker_process_cycle(serverfd, fds[1]);
        exit(0);
    }
    // master process.
    worker_process.pid = pid;
    close(fds[1]);

    info("[master] fork process success: #" << pid << ", channel=" << worker_process.channel << endl);
}

int find_the_required_worker(int serverfd, vector<WorkerProcess>& workers, int client_required_id, int pre_pid){
    // find the exactly worker.
    WorkerProcess* target = NULL;
    int channel = -1;
    for(vector<WorkerProcess>::iterator ite = workers.begin(); ite != workers.end(); ++ite){
        WorkerProcess& worker = *ite;
        if((client_required_id % worker.pid) == 0){
            target = &worker;
            break;
        }
    }
    // find the most idling worker.
    int pre_num = 0;
    for(vector<WorkerProcess>::iterator ite = workers.begin(); ite != workers.end(); ++ite){
        WorkerProcess& worker = *ite;
        if(worker.num_clients < max_clients_per_process && worker.pid != pre_pid){
            if(pre_num == 0 || worker.num_clients < pre_num){
                target = &worker;
                pre_num = worker.num_clients;
            }
        }
    }

    if(target == NULL){
        if(workers.size() < max_workers){
            // start a new worker
            WorkerProcess worker;
            start_worker_process(serverfd, worker);
            workers.push_back(worker);

            return find_the_required_worker(serverfd, workers, client_required_id, pre_pid);
        }
        else{
            err_exit("[master] all " << workers.size() << " workers overloaded!");
        }
    }
    info("[master] find a target to passing fd, pid=" << target->pid << ", num_clients=" << target->num_clients << endl);
    target->num_clients ++;

    return target->channel;
}
void on_channel_message(int serverfd, WorkerProcess& host, vector<WorkerProcess>& workers){
    int sock = host.channel;

    channel_msg msg;
    read_channel(sock, &msg, sizeof(channel_msg));
    host.num_clients = msg.total_client;

    cout << "[master] get a channel msg, pid=#" << host.pid << ", fd=" << msg.fd
        << ", total_client=" << msg.total_client
        << ", param=" << msg.param << ", command=" << msg.command << endl;
    if(msg.command == CMD_FD){
        // dispatch and re-dispatch fd
        int channel = find_the_required_worker(serverfd, workers, msg.param, host.pid);
        msg.command = CMD_FD;
        if(msg.fd == 0 || msg.fd == 1){
            info("[master][warning] ignore invalid fd to passing: " << msg.fd << endl);
            return;
        }
        write_channel(channel, &msg, sizeof(channel_msg));
        // direct close the fd, the target worker MUST take it or close it.
        close(msg.fd);
        host.num_clients--;
    }
    else if(msg.command == CMD_INFO){
        return;
    }
    else{
        err_exit("[master] invalid response!");
    }
}

int main(int argc, char** argv){
    info("[master] SIGRTMIN=0x" << hex << SIGRTMIN
        << ", SIGRTMAX=0x" << hex << SIGRTMAX << dec
        << ", SIGIO=0x" << hex << SIGIO << dec
        << ", SIGPOLL=0x" << hex << SIGPOLL << dec
        << endl);
    register_signal_handler();
    block_specified_signals();

    //sleep(3);
    UserOptions options;
    discovery_user_options(argc, argv, options);
    int serverfd = listen_server_socket(options);

    // single process mode
    if(options.num_processes == 0){
        worker_process_cycle(serverfd, -1);
    }
    // multiple processes mode
    else{
        vector<WorkerProcess> workers;
        for(int i = 0; i < options.num_processes; i++){
            WorkerProcess worker;
            start_worker_process(serverfd, worker);
            workers.push_back(worker);
        }

        // the empty set for sigsuspend
        sigset_t set;
        sigemptyset(&set);
        // master process use signal only.
        for(;;){
            sigsuspend(&set);

            if(global_terminate){
                // kill all workers.
                for(vector<WorkerProcess>::iterator ite = workers.begin(); ite != workers.end(); ++ite){
                    WorkerProcess& worker = *ite;
                    kill(worker.pid, SIGTERM);
                }
                // wait for workers to exit.
                for(vector<WorkerProcess>::iterator ite = workers.begin(); ite != workers.end(); ++ite){
                    WorkerProcess& worker = *ite;
                    int status;
                    waitpid(worker.pid, &status, 0);
                }
                info("[master] all worker terminated, master exit" << endl);
                exit(0);
            }
            if(global_channel_msg){
                int active_fd = current_active_io_fd;
                current_active_io_fd = -1;
                global_channel_msg = false;

                // read each channel.
                for(vector<WorkerProcess>::iterator ite = workers.begin(); ite != workers.end(); ++ite){
                    WorkerProcess& worker = *ite;
                    if(worker.channel == active_fd){
                        on_channel_message(serverfd, worker, workers);
                        break;
                    }
                }
            }
        }
    }

    return 0;
}

