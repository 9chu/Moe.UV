/**
 * @file
 * @author chu
 * @date 2017/12/11
 */
#pragma once
#include <Moe.Core/CircularQueue.hpp>

#include "IoHandle.hpp"
#include "EndPoint.hpp"
#include "CoEvent.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    class TcpListener;

    /**
     * @brief TCP套接字
     */
    class TcpSocket :
        public IoHandle
    {
        friend class ObjectPool;
        friend class TcpListener;

    public:
        using ConnectCallbackType = std::function<void(int)>;
        using ShutdownCallbackType = std::function<void(int)>;
        using ReadCallbackType = std::function<void(int, ObjectPool::BufferPtr, MutableBytesView)>;

        /**
         * @brief 创建TCP套接字
         */
        static IoHandleHolder<TcpSocket> Create();

    private:
        enum class ReadState
        {
            NotReading,
            Reading,
            Pending,
        };

        struct QueueData
        {
            ObjectPool::BufferPtr Holder;
            MutableBytesView View;
        };

        static void OnUVConnect(::uv_connect_t* req, int status)noexcept;
        static void OnUVShutdown(::uv_shutdown_t* req, int status)noexcept;
        static void OnUVWrite(::uv_write_t* req, int status)noexcept;
        static void OnUVDirectWrite(::uv_write_t* req, int status)noexcept;
        static void OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept;
        static void OnUVRead(::uv_stream_t* stream, ssize_t nread, const uv_buf_t* buf)noexcept;

    protected:
        TcpSocket();

    public:
        /**
         * @brief 获取本地地址
         *
         * 如果失败，返回空地址。
         */
        EndPoint GetLocalEndPoint()const noexcept;

        /**
         * @brief 获取远端地址
         *
         * 如果失败，返回空地址。
         */
        EndPoint GetRemoteEndPoint()const noexcept;

        /**
         * @brief 设置是否开启NoDelay
         */
        void SetNoDelay(bool enable);

        /**
         * @brief 设置是否开启TCP Keep-Alive机制
         * @param enable 是否启用
         * @param delay 初始等待时间，如果enable为false，则忽略该参数
         */
        void SetKeepAlive(bool enable, uint32_t delay);

        /**
         * @brief 判断是否可读
         */
        bool IsReadable()const noexcept;

        /**
         * @brief 判断是否可写
         */
        bool IsWritable()const noexcept;

        /**
         * @brief 绑定套接字
         * @param address 绑定地址
         * @param ipv6Only 是否仅IPV6地址
         */
        void Bind(const EndPoint& address, bool ipv6Only=false);

        /**
         * @brief 连接地址
         * @param address 地址
         * @param callback 回调函数
         */
        void Connect(const EndPoint& address, const ConnectCallbackType& callback);
        void Connect(const EndPoint& address, ConnectCallbackType&& callback);

        /**
         * @brief 启动读操作
         * @param callback 回调函数
         *
         * 开始接受数据包。
         * 当指定回调函数时，会通过回调函数通知数据包到来。
         * 当不指定回调函数时，数据包会被缓存到队列，此时可以通过协程读方法获取数据。
         */
        void StartRead(const ReadCallbackType& callback=ReadCallbackType());
        void StartRead(ReadCallbackType&& callback);

        /**
         * @brief 停止读取数据包
         *
         * 停止接受数据包。
         * 方法会清除回调函数，并通知等待在读事件上的协程退出。
         * 当有未读取的数据时会丢弃队列。
         *
         * 如果使用Close而不是CancelRead将会引发OperationCancelledException。
         */
        bool StopRead()noexcept;

        /**
         * @brief （无阻塞）发送数据包
         * @param buffer 数据（将被拷贝到内部缓冲区，调用后可以立即释放）
         */
        void Send(BytesView buffer);

        /**
         * @brief 关闭写端并等待数据全部写出
         * @param callback 回调
         */
        void Shutdown(const ShutdownCallbackType& callback);
        void Shutdown(ShutdownCallbackType&& callback);

        /**
         * @brief （协程）连接地址
         * @param address 地址
         *
         * 只能在单个协程调用。
         */
        void CoConnect(const EndPoint& address);

        /**
         * @brief （协程）接收数据包
         * @param[out] holder 数据缓冲区对象
         * @param[out] view 缓冲区
         * @param timeout 超时时间
         * @return 是否收到数据，若为false表示操作被取消或超时
         *
         * 该方法用于异步接收任意长度数据包，一旦有数据可读即触发。亦写作ReadSome。
         * 该方法将直接传出内部缓冲区和有效的读取区段，当holder被释放时内部缓冲区才会交还内存池。
         * 当读取到EOF也会产生false表明操作被取消。
         *
         * 只能在单个协程调用。
         */
        bool CoRead(ObjectPool::BufferPtr& holder, MutableBytesView& view,
            Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief （协程）读取若干字节
         * @param[in,out] target 目标缓冲区
         * @param[out] count 数量
         * @param timeout 超时时间
         * @return 是否收到数据，若为false表示操作被取消或超时
         *
         * 该方法会阻塞协程，直到读取到count个字节的数据。
         * 当读取到EOF也会产生false表明操作被取消。
         * 注意当超时时可能已完成部分数据的读取。
         *
         * 只能在单个协程调用。
         */
        bool CoRead(uint8_t* target, size_t count, Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief （协程）发送数据包并等待
         * @param address 地址
         * @param buffer 数据（无拷贝）
         *
         * 只能在单个协程调用。
         *
         * 注意：
         * - 协程版本buffer中的数据不会被拷贝
         * - buffer不能分配在协程栈上，必须分配在堆内存上
         */
        void CoSend(BytesView buffer);

        /**
         * @brief （协程）关闭写端并等待数据全部写出
         *
         * 只能在单个协程调用。
         */
        void CoShutdown();

    protected:
        void OnClose()noexcept override;
        void OnError(int error)noexcept;
        void OnSend(size_t len)noexcept;
        void OnRead(ObjectPool::BufferPtr buffer, size_t len)noexcept;
        void OnEof()noexcept;

    private:
        ::uv_tcp_t m_stHandle;

        ReadState m_eReadState = ReadState::NotReading;  // 读状态
        CircularQueue<QueueData, 8> m_stQueuedBuffer;  // 缓存的缓冲区

        // 协程
        CoEvent m_stReadEvent;  // 等待读的协程

        // 回调
        ReadCallbackType m_stReadCallback;
    };

    /**
     * @brief TCP监听器
     */
    class TcpListener :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using AcceptCallbackType = std::function<void(int, IoHandleHolder<TcpSocket>)>;

        /**
         * @brief 创建TCP监听器
         * @param address 地址
         * @param ipv6Only 是否只监听ipv6地址
         */
        static IoHandleHolder<TcpListener> Create();
        static IoHandleHolder<TcpListener> Create(const EndPoint& address, bool ipv6Only=false);

    private:
        static void OnUVNewConnection(::uv_stream_t* server, int status)noexcept;

    public:
        TcpListener();

    public:
        /**
         * @brief 获取本地地址
         *
         * 如果失败，返回空地址。
         */
        EndPoint GetLocalEndPoint()const noexcept;

        /**
         * @brief 绑定套接字
         * @param address 绑定地址
         * @param ipv6Only 是否仅IPV6地址
         */
        void Bind(const EndPoint& address, bool ipv6Only=false);

        /**
         * @brief 启动监听
         * @param callback 回调函数
         * @param backlog 队列大小
         */
        void Listen(const AcceptCallbackType& callback=AcceptCallbackType(), int backlog=128);
        void Listen(AcceptCallbackType&& callback, int backlog=128);

        /**
         * @brief （协程）等待并接受句柄
         * @param timeout 超时时间
         * @return 接受的Socket对象，若取消或者超时，返回nullptr
         */
        IoHandleHolder<TcpSocket> CoAccept(Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief 取消等待的协程
         */
        void CancelWait()noexcept;

    protected:
        void OnClose()noexcept override;
        void OnError(int error)noexcept;
        void OnNewConnection()noexcept;

    private:
        ::uv_tcp_t m_stHandle;

        // 协程
        CoEvent m_stAcceptEvent;  // 等待对象的协程

        // 回调
        AcceptCallbackType m_stAcceptCallback;
    };
}
}
