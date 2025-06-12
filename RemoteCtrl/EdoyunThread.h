#pragma once
#include "pch.h"
#include <atomic>
#include <vector>
#include <mutex>
#include <windows.h>

class ThreadFuncBase {};// 定义一个空基类，作为所有可在线程中执行的工作类的基类
/*
 定义统一的成员函数指针类型
 ThreadFuncBase* 是普通的​​对象指针​；ThreadFuncBase::* 是一个​​成员指针声明符​​的一部分，用于声明指向类的成员的指针（通常是成员函数指针）；
 必须与具体类型结合使用，比如 FUNCTYPE funcPtr = &DerivedClass::someMethod;
*/
typedef int (ThreadFuncBase::* FUNCTYPE)();


class ThreadWorker {
public:
    ThreadWorker() : thiz(NULL), func(NULL) {}

    ThreadWorker(void* obj, FUNCTYPE f) : thiz((ThreadFuncBase*)obj), func(f) {}

    ThreadWorker(const ThreadWorker& worker) {
        thiz = worker.thiz;
        func = worker.func;
    }

    ThreadWorker& operator=(const ThreadWorker& worker) {
        if (this != &worker) {
            thiz = worker.thiz;
            func = worker.func;
        }
        return *this;
    }

    // 函数调用运算符重载
    int operator()() {
        /*
        通过重载 operator() 使得类的对象可以像函数一样被调用。即仿函数(Functor)模式，比如：
        ThreadWorker worker(obj, &MyClass::doWork); // 创建工作单元
        int result = worker(); // 像调用函数一样使用对象
        */
        if (IsValid()) {
            return (thiz->*func)();
        }
        return -1;
    }

    // 检查工作单元是否有效（对象和函数指针都不为空）
    bool IsValid() const{
        return (thiz != NULL) && (func != NULL);
    }

private:
    ThreadFuncBase* thiz;
    FUNCTYPE func;
};

// 单个线程类
class EdoyunThread
{
public:
    EdoyunThread()
    {
        m_hThread = NULL;
        m_bStatus = false;
    }
    ~EdoyunThread() {
        Stop();
    }

    //线程启动
    bool Start() {
        m_bStatus = true; // 设置运行标志
        m_hThread = (HANDLE)_beginthread(&EdoyunThread::ThreadEntry, 0, this); // 从ThreadEntry进入 启动线程
        if (!IsValid()) { // 检查线程是否创建成功
            m_bStatus = false;
        }
        return m_bStatus;
    }

    // 线程状态检查
    bool IsValid() {//返回true表示有效 返回false表示线程异常或者已经终止
        if (m_hThread == NULL || (m_hThread == INVALID_HANDLE_VALUE))return false;
        return WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT;
    }

    // ​​线程停止
    bool Stop() {
        if (m_bStatus == false)return true;
        m_bStatus = false;
        DWORD ret =  WaitForSingleObject(m_hThread, 1000);
        if (ret == WAIT_TIMEOUT) {
            TerminateThread(m_hThread, -1);
        }
        UpdateWorker();
        return ret == WAIT_OBJECT_0;
    }

    // 工作分配​​ 
    void UpdateWorker(const ::ThreadWorker& worker = ::ThreadWorker()) {
        if (m_worker.load() != NULL && m_worker.load() != &worker) {
            ::ThreadWorker* pWorker = m_worker.load();
            m_worker.store(NULL);
            delete pWorker;
        }

        if (m_worker.load() == &worker) return;

        if (!worker.IsValid()) {
            m_worker.store(NULL);
            return;
        }
        ::ThreadWorker* pWorker = new ::ThreadWorker(worker);
        m_worker.store(pWorker);
    }

    //空闲检查 true表示空闲 false表示分配了工作
    bool IsIdle() {
        if (m_worker.load() == NULL)return true;
        return !m_worker.load()->IsValid();// 原子加载并检查
    }

private:
    // 线程工作循环
    void ThreadWorker() {
        while (m_bStatus) {
            if (m_worker.load() == NULL) {
                Sleep(1);
                continue;
            }
            ::ThreadWorker worker = *m_worker.load();// 加载当前任务 worker就是m_worker所管理的ThreadWorker对象的值拷贝（副本​）
            if (worker.IsValid()) {
                if (WaitForSingleObject(m_hThread, 0) == WAIT_TIMEOUT) {
                    int ret = worker();// 执行任务
                    if (ret != 0) {// 非零返回值处理
                        CString str;
                        str.Format(_T("thread found warning code %d\r\n"), ret);
                        OutputDebugString(str);
                    }
                    if (ret < 0) {// 任务请求结束
                        ::ThreadWorker* pWorker = m_worker.load();
                        m_worker.store(NULL);// 创建默认构造的ThreadWorker对象来覆盖原来的worker，逻辑上实现清空工作单元
                        delete pWorker;
                    }
                }
            }
            else {
                Sleep(1);// 短暂休眠避免忙等
            }
        }
    }
    // 线程入口 避免_endthread()提前关闭线程导致资源泄露
    static void ThreadEntry(void* arg) {
        EdoyunThread* thiz = (EdoyunThread*)arg;
        if (thiz) {
            thiz->ThreadWorker();
        }
        _endthread();
    }

private:
    HANDLE m_hThread;
    bool m_bStatus;//false 表示线程将要关闭  true 表示线程正在运行
    std::atomic<::ThreadWorker*> m_worker; // 原子变量，任何时候只能包含一个对象
};

// 线程池类
class EdoyunThreadPool
{
public:
    EdoyunThreadPool(size_t size) {
        m_threads.resize(size);
        for (size_t i = 0; i < size; i++) {
            m_threads[i] = new EdoyunThread();
        }
    }
    EdoyunThreadPool() {}
    ~EdoyunThreadPool() {
        Stop();
        for (size_t i = 0; i < m_threads.size(); i++) {
            delete m_threads[i];
            m_threads[i] = NULL;
        }
        m_threads.clear();
    }
    
    // 启动所有线程
    bool Invoke() {
        bool ret = true;
        for (size_t i = 0; i < m_threads.size(); i++) {
            if (m_threads[i]->Start() == false) {
                ret = false;
                break;
            }
        }
        if (ret == false) {
            for (size_t i = 0; i < m_threads.size(); i++) {
                m_threads[i]->Stop();
            }
        }
        return ret;
    }

    // 停止所有线程
    void Stop() {
        for (size_t i = 0; i < m_threads.size(); i++) {
            m_threads[i]->Stop();
        }
    }

    //分配工作 返回-1 表示分配失败，所有线程都在忙；大于等于0，表示第n个线程分配来做这个事情
    int DispatchWorker(const ThreadWorker& worker) {
        int index = -1;
        // 互斥操作 找到一个空闲的线程就指派任务 然后就返回
        m_lock.lock();
        for (size_t i = 0; i < m_threads.size(); i++) {
            if (m_threads[i]->IsIdle()) {
                m_threads[i]->UpdateWorker(worker);
                index = i;
                break;
            }
        }
        m_lock.unlock();
        return index;
    }

    // // 检查指定线程是否有效
    bool CheckThreadValid(size_t index) {
        if (index < m_threads.size()) {
            return m_threads[index]->IsValid();
        }
        return false;
    }
private:
    std::mutex m_lock;
    std::vector<EdoyunThread*> m_threads;
};

