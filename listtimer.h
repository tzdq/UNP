//
// Created by Administrator on 2017/3/25.
//

#ifndef UNP_LISTTIMER_H
#define UNP_LISTTIMER_H

#include "unp.h"
#include <time.h>
#include <iostream>
using namespace std;
class util_timer;

struct client_data{
    sockaddr_in address;
    int sockfd;
    char buf[MAX_BUFF_SIZE];
    util_timer *timer;
};

class util_timer{
public:
    util_timer():next(NULL),prev(NULL){}
public:
    time_t  expire;//任务的超时时间，使用绝对值
    client_data *user_data;//
    void (*cb_func)(client_data *user_data);//任务回调函数
    util_timer *next;
    util_timer *prev;
};
//升序定时器链表
class sort_timer_lst{
public:
    sort_timer_lst():head(NULL),tail(NULL){}
    ~sort_timer_lst(){
        util_timer *tmp = head;
        while(tmp){
            head = tmp->next;
            delete tmp;
            tmp = head;
        }
    }

    void add_timer(util_timer *timer){
        if(!timer)return;
        if(!head){//头结点为空，直接插入
            head = tail = timer;
            return;
        }
        //如果头结点的超时时间大于timer的超时时间，直接插入到头结点之前，作为新的头结点
        if(timer->expire < head->expire){
            timer->next = head;
            head->prev = timer;
            head = timer;
            return;
        }
        //保证升序，插入到合适的位置
        add_timer(timer,head);
    }

    //当某个定时任务发生变化时，调整对应的定时器在链表中的位置，目前是向后调整
    void adjust_timer(util_timer *timer){
        if(!timer)return;
        util_timer *tmp = timer->next;
        if(!tmp || timer->expire < tmp->expire)return;
        if(timer == head){
            head = head->next;
            head->prev = NULL;
            timer->next = NULL;
            add_timer(timer,head);
            return;
        }
        //如果timer位于中间
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer,timer->next);
    }

    void delete_timer(util_timer *timer){
        if(!timer)return;
        //链表中只有一个元素
        if(timer == head && timer == tail){
            head = tail = NULL;
            delete timer;
            return ;
        }
        if(timer == head){
            head = head->next;
            head->prev = NULL;
            delete timer;
            return;
        }
        if(timer == tail){
            tail = tail->prev;
            tail->next = NULL;
            delete timer;
            return;
        }
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        delete timer;
    }

    void tick(){
        if(!head)return;
        cout << "timer tick" << endl;
        time_t  cur = time(NULL);
        util_timer *tmp = head;
        while (tmp){
            if(cur < tmp->expire){
                break;
            }
            tmp->cb_func(tmp->user_data);
            head = tmp->next;
            if(head){
                head->prev = NULL;
            }
            delete tmp;
            tmp = head;
        }
    }

private:
    void add_timer(util_timer *timer,util_timer *lst_head){
        util_timer *prev = lst_head;
        util_timer *tmp = prev->next;
        while(tmp){
            if(timer->expire < tmp->expire){
                prev->next = timer;
                timer->next = tmp;
                timer->prev = prev;
                tmp->prev = timer;
                break;
            }
            prev = tmp;
            tmp = tmp->next;
        }
        if(!tmp){
            prev->next = timer;
            timer->prev = prev;
            timer->next = NULL;
            tail = timer;
        }
    }
private:
    util_timer *head;
    util_timer *tail;
};
//时间轮
class tw_timer;
struct tw_client_data{
    sockaddr_in address;
    int sockfd;
    char buf[MAX_BUFF_SIZE];
    tw_timer *timer;
};
class tw_timer{
public:
    tw_timer(int rol,int ts):rolation(rol),timeslot(ts),next(NULL),prev(NULL){}

public:
    int rolation;//记录定时器在时间轮转多少圈后生效
    int timeslot;//所属槽
    tw_timer *next,*prev;
    void (*cb_func)(tw_client_data *user);
    tw_client_data *user_data;
};

class time_wheel{
    time_wheel():cur_slot(0){
        for (int i = 0; i < N; ++i) {
            slots[i] = NULL;
        }
    }
    ~time_wheel(){
        for (int i = 0; i < N; ++i) {
            tw_timer* tmp = slots[i];
            while (tmp){
                slots[i] = tmp->next;
                delete tmp;
                tmp = slots[i];
            }
        }
    }

    tw_timer* add_timer(int timeout){
        if(timeout < 0)return NULL;
        int ticks = 0;
        if(timeout < SI)ticks = 1;
        else ticks = timeout / SI;

        int rolation = ticks / N;
        int ts = ((cur_slot + (ticks %N))%N);
        tw_timer *timer = new tw_timer(rolation,ts);
        if(!slots[ts]){
            //对应的槽没有定时器
            slots[ts] = timer;
            cout <<"add timer,rolation = " << rolation <<"ts = " << ts << "cur_slot = "<< cur_slot<<endl;
        }
        else{
            timer->next = slots[ts];
            slots[ts]->prev = timer;
            slots[ts] = timer;
        }
        return timer;
    }

    void delete_timer(tw_timer *timer){
        if(!timer)return ;
        int ts = timer->timeslot ;
        if(timer == slots[ts]){
            slots[ts] = slots[ts]->next;
            if(slots[ts]){
                slots[ts]->prev = NULL;
            }
            delete timer;
        }
        else{
            timer->prev->next = timer->next;
            if(timer->next){
                timer->next->prev = timer->prev;
            }
            delete timer;
        }
    }

    void tick(){
        tw_timer *tmp = slots[cur_slot];
        while(tmp){
            if(tmp->rolation > 0){
                tmp->rolation--;
                tmp = tmp->next;
            }
            else{
                tmp->cb_func(tmp->user_data);
                if(tmp == slots[cur_slot]){
                    slots[cur_slot] = tmp->next;
                    delete tmp;
                    if(slots[cur_slot]){
                        slots[cur_slot]->prev = NULL;
                    }
                    tmp = slots[cur_slot];
                }
                else{
                    tmp->prev->next = tmp->next;
                    if(tmp->next){
                        tmp->next->prev = tmp->prev;
                    }
                    tw_timer *tmp1 = tmp->next;
                    delete tmp;
                    tmp = tmp1;
                }
            }
        }
        cur_slot = ++cur_slot % N;
    }

private:
    int cur_slot;
    static const int N = 60;
    static const int SI = 1;
    tw_timer* slots[N];
};
//时间堆
class heap_timer;
struct heap_client_data{
    sockaddr_in address;
    int sockfd;
    char buf[MAX_BUFF_SIZE];
    heap_timer *timer;
};

class heap_timer{
public:
    heap_timer(int delay){
        expire = time(NULL) + delay;
    }
    time_t expire;
    void (*cb_func)(heap_client_data *user_data);
    heap_client_data *user_data;
};

class time_heap{
public:
    time_heap(int cap)throw (std::exception):capacity(cap),cur_size(0){
        array = new heap_timer* [capacity];
        if(!array)throw std::exception();
        for(int i = 0 ;i < capacity;++i){
            array[i] = NULL;
        }
    }
    time_heap(heap_timer **arr,int cap,int size)throw (std::exception):capacity(cap),cur_size(size){
        if(capacity < cur_size)throw  std::exception();

        array = new heap_timer* [capacity];
        if(!array)throw std::exception();
        for(int i = 0 ;i < capacity;++i){
            array[i] = NULL;
        }
        if(size != 0){
            for(int  i = 0 ;i < size;++i){
                array[i] = arr[i];
            }
            for(int i = (cur_size-1)/2;i>=0;--i){
                percolate_down(i);
            }
        }
    }

    ~time_heap(){
        for (int i = 0; i < cur_size; ++i) {
            delete array[i];
        }
        delete []array;
    }

    void add_timer(heap_timer *timer){
        if(!timer)return;
        if(capacity <= cur_size){
            resize();
        }
        int hole = cur_size++;
        int parent = 0;
        for(;hole > 0;hole = parent){
            parent = (hole -1)/2;
            if(array[parent]->expire <= timer->expire)break;
            array[hole] = array[parent];
        }
        array[parent] = timer;
    }

    void delete_timer(heap_timer *timer){
        if(!timer)return ;
        timer->cb_func = NULL;
    }

    heap_timer* top()const{
        if(empty())return NULL;
        return array[0];
    }

    bool empty()const {return cur_size == 0;}

    void pop_timer(){
        if(empty())return;
        if(array[0]){
            delete array[0];
            array[0] = array[--cur_size];
            percolate_down(0);
        }
    }

    void tick(){
        heap_timer *tmp = array[0];
        time_t cur = time(NULL);
        while (tmp){
            if(!tmp)break;
            if(tmp->expire > cur)break;
            if(tmp->cb_func){
                tmp->cb_func(tmp->user_data);
            }
            pop_timer();
            tmp = array[0];
        }
    }

private:
    void percolate_down(int hole){
        heap_timer *tmp = array[hole];
        int child = 0;
        for(;(hole*2+1) < cur_size;hole = child){
            child = hole * 2 + 1;
            if((child < cur_size-1) && (array[child+1]->expire < array[child]->expire))++child;
            if(array[child]->expire < tmp->expire)
                array[hole] = array[child];
            else break;
        }
        array[hole] = tmp;
    }

    void resize()throw(std::exception){
        heap_timer ** tmp = new heap_timer*[2*capacity];
        if(!tmp)throw  std::exception();
        for(int i = 0 ;i < 2*capacity;++i){
            tmp[i] = NULL;
        }
        capacity = capacity*2;
        for(int i = 0 ;i < cur_size;i++){
            tmp[i] = array[i];
        }
        delete []array;
        array = tmp;
    }
private:
    heap_timer **array;
    int capacity;//堆数组大小
    int cur_size;//堆数组中当前元素个数
};
#endif //UNP_LISTTIMER_H
