# RemoteCtrl

> 这是一个个人学习项目，用于学习 **Windows 网络编程（Winsock）**、**MFC 图形界面开发**、**多线程编程**、**IOCP 完成端口**、**UDP 打洞/NAT 穿透** 等技术栈。

---

## 项目概述

RemoteCtrl 是一套基于 C++/MFC 实现的 Windows 远程控制系统，采用经典的 **Client-Server** 架构。被控端（Server）运行在目标机器上，监听连接并响应控制指令；控制端（Client）连接到被控端，发出指令并展示反馈结果。

整个项目以渐进式迭代的方式演进，每个分支代表一个独立的技术探索阶段，从最基础的 TCP Socket 通信起步，逐步引入代码重构、IOCP 高性能服务器、UDP 打洞等进阶内容。

---

## 整体架构

```
RemoteCtrl.sln
├── RemoteCtrl/        被控端（Server）—— 运行于目标机器
│   ├── ServerSocket   TCP 服务端套接字封装，负责监听、接入客户端、收发数据包
│   └── Command        命令处理模块，将命令号映射到具体处理函数
└── RemoteClient/      控制端（Client）—— 操作者使用的界面程序
    ├── ClientSocket   TCP 客户端套接字封装，负责连接服务端、收发数据包
    └── RemoteClientDlg MFC 对话框主界面，包含文件树、文件列表、远程监控等 UI
```

通信采用自定义二进制协议，数据包结构（`CPacket`）如下：

| 字段 | 大小 | 说明 |
|------|------|------|
| `sHead` | 2 字节 | 固定包头 `0xFEFF`，用于同步 |
| `nLength` | 4 字节 | 包体长度（从命令字段到校验和） |
| `sCmd` | 2 字节 | 命令号，标识操作类型 |
| `strData` | 变长 | 命令附带数据 |
| `sSum` | 2 字节 | 和校验，保证数据完整性 |

---

## 核心功能与技术要点

### 通信协议
- 自定义二进制分包协议，处理 TCP 粘包/半包问题
- 包头同步机制（`0xFEFF`）+ 和校验，确保数据可靠性
- `#pragma pack(1)` 内存对齐控制，保证跨平台结构体布局一致

### 命令体系

| 命令号 | 功能 |
|--------|------|
| 1 | 获取磁盘分区列表 |
| 2 | 获取指定目录下的文件列表 |
| 3 | 远程运行文件（`ShellExecute`） |
| 4 | 下载文件（支持大文件分块传输） |
| 5 | 鼠标事件透传（移动/单击/双击/按下/抬起，左中右键） |
| 6 | 屏幕截图并以 PNG 编码发送（GDI+ / `CImage`） |
| 7 | 锁机（全屏遮罩 + 限制鼠标 + 隐藏任务栏） |
| 8 | 解锁（线程间消息通信 `PostThreadMessage`） |
| 9 | 删除远端文件 |
| 1981 | 连接测试心跳 |

### 单例模式
`CServerSocket` 与 `CClientSocket` 均使用 Meyers-like 手动单例（含嵌套 `CHelper` 生命周期管理类），保证全局唯一实例。

### MFC 界面
- 控制端使用 MFC 对话框程序，包含 `CTreeCtrl`（目录树）、`CListCtrl`（文件列表）及右键上下文菜单
- 屏幕监控使用独立线程接收帧数据并通过 `CImage` 渲染到窗口
- 自定义消息 `WM_SEND_PACKET` 实现工作线程向 UI 线程的安全回调

### 多线程
- 被控端采用独立线程处理命令，避免 UI 阻塞
- 控制端使用独立线程接收屏幕数据流和文件下载流
- 锁机功能使用 `_beginthreadex` 创建独立 UI 线程以支持消息循环

### Windows API
- `GDI/GDI+`：屏幕截图、图像编码（PNG）、位图渲染
- `mouse_event`：鼠标事件模拟
- `_findfirst` / `_findnext`：目录遍历
- `ShellExecute`：远程启动程序
- `ClipCursor` / `ShowCursor`：锁机时限制鼠标

---

## 分支说明

### `master` — 基础功能完整版

最初始的可运行版本。完整实现了上述所有核心命令，`ServerSocket` 和 `ClientSocket` 以单文件类的形式封装了所有网络逻辑（`DealCommand` 阻塞式收包循环），UI 与网络逻辑耦合度较高。这是后续所有分支的起点。

---

### `client-debug` — 客户端 Debug 与注释完善

从 `master` 拆出，专注于**对客户端代码进行系统性调试和注释补全**。

- 修复了远程监控过程中的客户端崩溃 Bug
- 修复多线程导致的显示命令与鼠标命令冲突问题（程序卡死）
- 修复屏幕内容变化过大时接收端卡死的问题
- 修复目录信息获取的 Bug
- 对客户端核心流程添加了详细的中文注释，便于理解学习
- 发现并记录了资源访问的多线程安全性问题（未完全解决）

---

### `reconstruct` — 代码架构重构

在 `client-debug` 基础上对整体架构进行重构，核心目标是**解耦网络层与业务层**。

- 服务端新增独立的 `CCommand` 类，将全部命令处理逻辑从 `ServerSocket` 中剥离，以 `std::map<int, CMDFUNC>` 命令号到成员函数指针的方式进行命令分发
- 客户端新增 `ClientController` 类，将下载线程、屏幕监控线程从 `RemoteClientDlg` 中移出，降低界面层与控制逻辑的耦合
- 引入 `EdoyunTool` 工具类（管理员权限检测、开机自启、错误信息显示）
- 服务端实现开机自启动功能（复制自身到 Startup 目录）
- 服务端实现管理员权限自动检测与提权请求
- 修复多个内存泄漏及启动崩溃问题

---

### `iocp` — IOCP 高性能服务器

在重构版本基础上，将服务端网络层升级为 **Windows IOCP（I/O Completion Port，I/O 完成端口）** 模型，是本项目网络编程深度最高的分支。

- 实现 `EdoyunServer`：基于 IOCP 的异步 TCP 服务器，管理多客户端连接
- 实现 `EdoyunClient`：每个连接对应一个客户端对象，持有独立的收发 Overlapped 结构
- 实现 `EdoyunOverlapped` 体系：`AcceptOverlapped` / `RecvOverlapped` / `SendOverlapped`，将异步 I/O 操作与回调绑定
- 实现 `EdoyunThread` / `EdoyunThreadPool`：自封装线程类与线程池，支持 `ThreadWorker`（仿函数）动态分配任务，使用原子变量保证线程安全
- 实现 `CEdoyunQueue<T>`：基于 IOCP 完成端口实现的线程安全队列（以 IOCP 替代传统锁），支持 `EdoyunSendQueue<T>` 异步发送队列
- 引入 `dbghelp.dll` / VLD（Visual Leak Detector）进行内存泄漏检测
- 修复服务器多处 Bug 及内存泄漏问题

---

### `udphole` — UDP 打洞 / NAT 穿透

在 IOCP 版本基础上，引入 **UDP 打洞**演示，探索 NAT 穿透的基本原理。

- 新增 `ESock.h`：对 `SOCKET`、`sockaddr_in`、收发缓冲区（`EBuffer`）等进行 C++ 封装，支持 TCP 和 UDP 两种类型（`ETYPE` 枚举）
- 新增 `ENetwork` / `EServer` / `EServerParameter`：使用运算符重载（`<<` / `>>`）实现流式参数配置的网络服务器抽象层，同时支持 TCP 和 UDP 服务
- `EServer` 内部区分 TCP 线程（`threadTCPFunc`）与 UDP 线程（`threadUDPFunc`），通过回调函数指针（`AcceptFunc`、`RecvFunc`、`RecvFromFunc` 等）将事件暴露给上层
- 演示了 UDP 打洞的基本流程与原理

---

### `design` — UML 设计文档分支

存放系统的 UML 设计文档（`远程控制系统设计.uml`），记录整体系统在设计阶段的类图与交互图，作为架构设计的参考留档。

---

## 项目演进路线

```
master（基础实现）
  └─► client-debug（客户端调试与注释）
        └─► reconstruct（架构解耦重构）
              └─► iocp（IOCP 高性能服务器）
                    └─► udphole（UDP 打洞 / NAT 穿透）
```
