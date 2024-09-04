#ifndef THREADPOOL_H
#define THREADPOOL_H
using namespace std;

#include "locker.h"
#include <cstdio>
#include <pthread.h>
#include <exception>
#include <list>
#include <iostream>

// 模板类为了代码的通用,模板参数T是任务类
template<typename T>
class threadpool{
public:
    threadpool(int thread_number = 8,int max_requests = 10000);
    ~threadpool();
    bool append(T* request);

private:
    // 工作线程运行的函数，它不断从工作队列中取出任务并执行之
    static void* worker(void* arg);
    void run();
private:
    // 线程数量
    int m_thread_number;
    
    // 线程池数组,大小为m_thread_number
    pthread_t* m_threads;

    //请求队列中最多允许的等待请求的数量
    int m_max_requests;
    
    //请求队列
    list<T* > m_workqueue; 

    // 保护请求队列的互斥锁；
    locker m_queuelocker;

    // 是否有任务需要处理
    sem m_queuestat;

    // 是否结束线程
    bool m_stop;

};

template<class T>
threadpool<T> :: threadpool(int thread_number ,int max_requests):
    m_thread_number(thread_number),m_max_requests(max_requests),
    m_stop(false),m_threads(NULL){

    if(thread_number <= 0 || max_requests <= 0){
        throw exception();
    }

    m_threads = new pthread_t[m_thread_number];
    if(!m_threads){
        throw exception();
    }

    //创建thread_number 个线程，并将他们设置为脱离线程
    for(int i = 0;i < thread_number;i++){
        printf("create the %dth thread\n",i);
        // cout << "create the" << i << "th thread\n";
        if(pthread_create(m_threads + i,NULL,worker,this) != 0){
            delete[] m_threads;
            throw exception();
        }

        if(pthread_detach(m_threads[i]) != 0){
            delete[] m_threads;
            throw exception();
        }
    }
}

template<class T>
threadpool<T>::~threadpool(){
    delete[] m_threads;
    m_stop = true;
}

template<typename T>
bool threadpool<T>::append(T* request){
    m_queuelocker.lock();
    if(m_workqueue.size() >= m_max_requests){
        m_queuelocker.unlock();
        return false;
    }
    m_workqueue.push_back(request);
    m_queuelocker.unlock();
    m_queuestat.post();
    return true;
}

template<class T>
void* threadpool<T>::worker(void* arg){
    threadpool* pool = (threadpool*) arg;
    pool->run();
    return pool;
}

template<class T>
void threadpool<T>::run(){
    while(!m_stop){
        m_queuestat.wait();
        m_queuelocker.lock();
        if(m_workqueue.empty()){ 
            m_queuelocker.unlock();
            continue;
        }
        T* request = m_workqueue.front();
        m_workqueue.pop_front();
        m_queuelocker.unlock();
        if(!request)
            continue;
        request->process();
    }
}

#endif