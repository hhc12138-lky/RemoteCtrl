#pragma once
template<class T>  // <T>提供IntelliSense的示例

class CEdoyunQueue
{
// 线程安全的队列（利用IOCP实现）
public:
    CEdoyunQueue();
    ~CEdoyunQueue();

    // 核心接口
    bool PushBack(const T& data);  // 向队列尾部压入数据
    bool PopFront(T& data);        // 从队列头部弹出数据
    size_t Size();                 // 获取队列当前大小
    void Clear();                  // 清空队列

private:
    // 线程相关函数
    static void threadEntry(void* arg);  // 静态线程入口函数
    void threadMain();                   // 实际线程逻辑

private:
    std::list<T> m_lstData;        // 底层数据存储（双向链表）
    HANDLE m_hCompletionPort;      // IOCP完成端口句柄（注意原图拼写错误：m_hCompeletionPort）
    HANDLE m_hThread;              // 工作线程句柄

public:
    // I/O完成端口任务参数结构体（用于线程间通信）
    typedef struct IocpParam {
        int nOperator;                // 操作类型标识（对应下方枚举值）
        T strData;                    // 传输的数据（模板类型T，图中示例为const char*）
        HANDLE hEvent;                // 用于EQPop操作的同步事件句柄

        // 带参构造函数（投递任务时使用）
        IocpParam(int op, const char* sData, _beginthread_proc_type cb = NULL) {
            nOperator = op;
            strData = sData;
        }

        // 无参构造函数（默认初始化）
        IocpParam() {
            nOperator = -1;  // 无效操作标识
        }
    } PPARAM;  // 类型别名：Post Parameter（投递参数）

    // 队列操作类型枚举（与nOperator字段对应）
    enum {
        EQPush,   // 压入数据
        EQPop,    // 弹出数据（需配合hEvent同步）
        EQSize,   // 获取队列大小
        EQClear   // 清空队列
    };

};




