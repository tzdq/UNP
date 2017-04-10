//
// Created by Administrator on 2017/3/30.
//
#include "unp.h"
#include <sys/sem.h>
#include <sys/wait.h>

union  semun{
    int val;
    struct semid_ds* buf;
    unsigned  short int* array;
    struct seminfo* __buf;
};

void pv(int sem_id,int op){
    struct sembuf sops;
    sops.sem_num = 0;
    sops.sem_op = op;
    sops.sem_flg = SEM_UNDO;
    semop(sem_id,&sops,1);
}

int main(int argc,char **argv){
    int sem_id = semget(IPC_PRIVATE,1,0666);
    union semun un;
    un.val = 1;
    semctl(sem_id,0,SETVAL,un);

    pid_t  pid = fork();
    if(pid < 0){
        return 1;
    }
    else if(pid == 0){
        cout << "child try to get bin sem" << endl;
        pv(sem_id,-1);
        cout <<"child get the sem after 5s"<<endl;
        sleep(5);
        pv(sem_id,1);
        exit(0);
    }
    else {
        cout << "parent try to get bin sem" << endl;
        pv(sem_id,-1);
        cout <<"parent get the sem after 5s"<<endl;
        sleep(5);
        pv(sem_id,1);
    }

    waitpid(pid,NULL,0);
    semctl(sem_id,1,IPC_RMID,un);
    return 0;
}

