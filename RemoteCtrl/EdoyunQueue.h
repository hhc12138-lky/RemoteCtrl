#pragma once
#include "pch.h"
#include<mutex>
#include <list>
#include <atomic>
#include "EdoyunThread.h"

//基于IOCP(Input / Output Completion Port)的线程安全队列框架
template<class T> 
class CEdoyunQueue
{
public:
    // 队列操作类型枚举（与nOperator字段对应）
    enum {
        EQNone,    // 无效操作标识
        EQPush,   // 压入数据
        EQPop,    // 弹出数据（需配合hEvent同步）
        EQSize,   // 获取队列大小
        EQClear   // 清空队列
    };

    // I/O完成端口 任务参数结构体（用于线程间通信）
    typedef struct IocpParam {
        size_t nOperator;                // 操作类型标识（对应下方枚举值）
        T Data;                    // 传输的数据（模板类型T，图中示例为const char*）
        HANDLE hEvent;                // 用于EQPop操作的同步事件句柄

        // 带参构造函数（投递任务时使用）
        IocpParam(int op, const T& data, HANDLE hEve = NULL) {
            nOperator = op;
            Data = data;
            hEvent = hEve;
        }

        // 无参构造函数（默认初始化）
        IocpParam() {
            nOperator = EQNone;  // 无效操作标识
        }
    } PPARAM;  // 类型别名：Post Parameter（投递参数）

public:
    // 构造函数中创建IOCP端口和工作线程
    CEdoyunQueue() {
        m_lock = false;
        m_hCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, NULL, 1);
        m_hThread = INVALID_HANDLE_VALUE;
        if (m_hCompletionPort != NULL) {

            m_hThread = (HANDLE)_beginthread(
                &CEdoyunQueue<T>::threadEntry,
                0, this
            );

        }
    }
    virtual ~CEdoyunQueue() {
        if (m_lock) return;
        m_lock = true;
        PostQueuedCompletionStatus(m_hCompletionPort, 0, NULL, NULL); // 向IOCP发送退出信号
        WaitForSingleObject(m_hThread, INFINITE);
        if (m_hCompletionPort != NULL) {
            HANDLE hTemp = m_hCompletionPort;
            m_hCompletionPort = NULL;
            CloseHandle(hTemp);
        }
    }

    // 线程安全地向队列尾部添加数据
    bool PushBack(const T& data) {
        IocpParam* pParam = new IocpParam(EQPush, data);
        if (m_lock) {
            delete pParam;
            return false;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)pParam, NULL);
        if (ret == false) delete pParam;
        return ret;
    }
    // 线程安全地从队列头部取出数据
    virtual bool PopFront(T& data) {
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        IocpParam Param(EQPop, data, hEvent);
        if (m_lock) {
            if (hEvent) CloseHandle(hEvent);
            return false;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM), (ULONG_PTR)&Param, NULL);
        if (ret == false) {
            CloseHandle(hEvent);
            return false;
        }
        ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
        if (ret) {
            data = Param.Data;
        }
        return ret;
    }
    // 获取当前队列大小
    size_t Size() {
        HANDLE hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        IocpParam Param(EQSize, T(), hEvent);
        if (m_lock) {
            if (hEvent)CloseHandle(hEvent);
            return -1;
        }
        bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
            (ULONG_PTR)&Param, NULL);
        if (ret == false) {
            CloseHandle(hEvent);
            return -1;
        }
        ret = WaitForSingleObject(hEvent, INFINITE) == WAIT_OBJECT_0;
        if (ret) {
            return Param.nOperator;
        }
        return -1;
    }
    // 清空队列
    bool Clear() {
        if (m_lock)return false;
        IocpParam* pParam = new IocpParam(EQClear, T());
        bool ret = PostQueuedCompletionStatus(m_hCompletionPort, sizeof(PPARAM),
            (ULONG_PTR)pParam, NULL);
        if (ret == false)delete pParam;
        return ret;
    }

protected:
    // 静态线程入口函数
    static void threadEntry(void* arg) {
        CEdoyunQueue<T>* thiz = (CEdoyunQueue<T>*)arg;
        thiz->threadMain();
        _endthread();
    }

    // 处理对应指令
    virtual void DealParam(typename CEdoyunQueue<T>::PPARAM* pParam) {
        switch (pParam->nOperator)
        {
        case EQPush:
            m_lstData.push_back(pParam->Data);
            delete pParam;
            break;
        case EQPop:
            if (m_lstData.size() > 0) {
                pParam->Data = m_lstData.front();
                m_lstData.pop_front();
            }
            if (pParam->hEvent != NULL)SetEvent(pParam->hEvent);
            break;
        case EQSize:
            pParam->nOperator = m_lstData.size();
            if (pParam->hEvent != NULL)
                SetEvent(pParam->hEvent);
            break;
        case EQClear:
            m_lstData.clear();
            delete pParam;
            break;
        default:
            OutputDebugString(_T("unknown operator!\r\n"));
            break;
        }
    }

    // 工作线程从IOCP获取任务并执行相应操作
    virtual void threadMain() {
        DWORD dwTransferred = 0;	// 接收I/O操作实际传输的字节数
        PPARAM* pParam = NULL;               // 自定义任务参数结构体的二级指针（用于线程间通信）
        ULONG_PTR CompletionKey = 0;         // IOCP完成键（通常传递对象指针或会话标识）
        OVERLAPPED* pOverlapped = NULL;      // 重叠I/O操作结构指针（用于异步I/O）
        while (GetQueuedCompletionStatus(
            m_hCompletionPort,
            &dwTransferred,
            &CompletionKey,
            &pOverlapped,
            INFINITE))
        {
            if ((dwTransferred == 0) || (CompletionKey == NULL))
            {
                printf("thread is prepare to exit!\r\n");
                break;
            }

            pParam = (PPARAM*)CompletionKey;
            DealParam(pParam);
        }

        // 防御编程
        while (GetQueuedCompletionStatus(m_hCompletionPort,&dwTransferred,&CompletionKey,&pOverlapped,0)) 
        {
            if ((dwTransferred == 0) || (CompletionKey == NULL))
            {
                printf("thread is prepare to exit!\r\n");
                continue;
            }
            pParam = (PPARAM*)CompletionKey;
            DealParam(pParam);
        }
        HANDLE hTemp = m_hCompletionPort;
        m_hCompletionPort = NULL;
        CloseHandle(hTemp);

    }

protected:
    std::list<T> m_lstData;        // 底层数据存储（双向链表）
    HANDLE m_hCompletionPort;      // IOCP完成端口句柄（注意原图拼写错误：m_hCompletionPort）
    HANDLE m_hThread;              // 工作线程句柄
    std::atomic<bool> m_lock;//队列正在析构
};



// 继承自CEdoyunQueue，增加了回调机制和自动处理功能。
template<class T>
class EdoyunSendQueue : public CEdoyunQueue<T>, public ThreadFuncBase
{ 
public:
    // 定义回调函数类型
    typedef int (ThreadFuncBase::* EDYCALLBACK)(T& data); 
    //启动工作线程并设置处理函数
    EdoyunSendQueue(ThreadFuncBase* obj, EDYCALLBACK callback): CEdoyunQueue<T>(), m_base(obj), m_callback(callback)
    {
        m_thread.Start();
        m_thread.UpdateWorker(::ThreadWorker(this, (FUNCTYPE) & EdoyunSendQueue<T>::threadTick));
    }
    virtual ~EdoyunSendQueue(){
        m_base = NULL;
        m_callback = NULL;
        m_thread.Stop();
    }
protected:
    virtual bool PopFront(T& data) { return false; };
    // 重写获取数据方法
    bool PopFront() {
        typename CEdoyunQueue<T>::IocpParam* Param = new typename CEdoyunQueue<T>::IocpParam(CEdoyunQueue<T>::EQPop, T());
        if (CEdoyunQueue<T>::m_lock) {
            delete Param;
            return false;
        }
        bool ret = PostQueuedCompletionStatus(CEdoyunQueue<T>::m_hCompletionPort, sizeof(*Param), (ULONG_PTR)&Param, NULL);
        if (ret == false) {
            delete Param;
            return false;
        }
        return ret;
    }

    // 线程循环处理函数，自动处理队列数据
    int threadTick()
    {
        if (WaitForSingleObject(CEdoyunQueue<T>::m_hThread, 0) != WAIT_TIMEOUT)
            return 0;
        if (CEdoyunQueue<T>::m_lstData.size() > 0) {
            PopFront();
        }
        return 0;
    }
    // 在DealParam中调用用户定义的回调函数
    virtual void DealParam(CEdoyunQueue<T>::PPARAM* pParam) {
        switch (pParam->nOperator)
        {
        case CEdoyunQueue<T>::EQPush:
            CEdoyunQueue<T>::m_lstData.push_back(pParam->Data);
            delete pParam;
            break;
        case CEdoyunQueue<T>::EQPop:
            if (CEdoyunQueue<T>::m_lstData.size() > 0) {
                pParam->Data = CEdoyunQueue<T>::m_lstData.front();
                if ((m_base->*m_callback)(pParam->Data) == 0) {
                    CEdoyunQueue<T>::m_lstData.pop_front();
                }
            }
            delete pParam;
            break;
        case CEdoyunQueue<T>::EQSize:
            pParam->nOperator = CEdoyunQueue<T>::m_lstData.size();
            if (pParam->hEvent != NULL)
                SetEvent(pParam->hEvent);
            break;
        case CEdoyunQueue<T>::EQClear:
            CEdoyunQueue<T>::m_lstData.clear();
            delete pParam;
            break;
        default:
            OutputDebugString(_T("unknown operator!\r\n"));
            break;
        }
    }

private:
    ThreadFuncBase* m_base;      // 线程函数基类指针（多态调用）
    EDYCALLBACK m_callback;       // 回调函数成员（类型别名或函数指针）
    EdoyunThread m_thread;        // 线程控制对象（封装线程操作）

};


