/**
 * @file
 * @author chu
 * @date 2017/12/8
 */
#pragma once
#include <functional>
#include <Moe.Core/CircularQueue.hpp>

#include "IoHandle.hpp"
#include "EndPoint.hpp"
#include "CoEvent.hpp"

namespace moe
{
namespace UV
{
    /**
     * @brief UDP套接字
     */
    class UdpSocket :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using ReadCallbackType = std::function<void(int, const EndPoint&, ObjectPool::BufferPtr, size_t)>;

        /**
         * @brief 构造UDP套接字
         * @param bind 绑定端口
         * @param reuse 是否复用
         * @param ipv6Only 是否仅绑定IPV6地址
         */
        static IoHandleHolder<UdpSocket> Create();
        static IoHandleHolder<UdpSocket> Create(const EndPoint& bind, bool reuse=true, bool ipv6Only=false);

    private:
        enum class ReadState
        {
            NotReading,
            Reading,
            Pending,
        };

        struct QueueData
        {
            EndPoint Remote;
            ObjectPool::BufferPtr Buffer;
            size_t Length;
        };

        static void OnUVSend(::uv_udp_send_t* request, int status)noexcept;
        static void OnUVDirectSend(::uv_udp_send_t* request, int status)noexcept;
        static void OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept;
        static void OnUVRecv(::uv_udp_t* udp, ssize_t nread, const ::uv_buf_t* buf, const ::sockaddr* addr,
            unsigned flags)noexcept;

    protected:
        UdpSocket();

    public:
        /**
         * @brief 获取本地地址
         *
         * 如果失败，返回空地址。
         */
        EndPoint GetLocalEndPoint()const noexcept;

        /**
         * @brief 设置数据包生存时间
         * @param ttl time-to-live时间
         */
        void SetTTL(int ttl);

        /**
         * @brief 绑定套接字
         * @param address 绑定地址
         * @param reuse 是否复用
         * @param ipv6Only 是否仅IPV6地址
         */
        void Bind(const EndPoint& address, bool reuse=true, bool ipv6Only=false);

        /**
         * @brief 开始接收数据包
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
         * @param address 地址
         * @param buffer 数据（将被拷贝到内部缓冲区，调用后可以立即释放）
         *
         * 如果尚未绑定端口，则会绑定到0.0.0.0和随机端口上。
         */
        void Send(const EndPoint& address, BytesView buffer);

        /**
         * @brief （协程）接收数据包
         * @param[out] remote 远端地址
         * @param[out] buffer 数据缓冲区
         * @param[out] size 数据大小
         * @param timeout 超时时间
         * @return 是否收到数据，若为false表示操作被取消或者超时
         *
         * 如果尚未绑定端口，则会绑定到0.0.0.0和随机端口上。
         * 仅允许单个协程调用。
         */
        bool CoRead(EndPoint& remote, ObjectPool::BufferPtr& buffer, size_t& size,
            Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief （协程）发送数据包并等待
         * @param address 地址
         * @param buffer 数据（无拷贝）
         *
         * 如果尚未绑定端口，则会绑定到0.0.0.0和随机端口上。
         * 仅允许单个协程调用。
         *
         * 注意：
         * - 协程版本buffer中的数据不会被拷贝
         * - buffer不能分配在协程栈上，必须分配在堆内存上
         */
        void CoSend(const EndPoint& address, BytesView buffer);

    protected:
        void OnClose()noexcept override;
        void OnError(int error)noexcept;
        void OnSend(size_t len)noexcept;
        void OnRead(const EndPoint& remote, ObjectPool::BufferPtr buffer, size_t len)noexcept;

    private:
        ::uv_udp_t m_stHandle;

        ReadState m_eReadState = ReadState::NotReading;  // 读状态
        CircularQueue<QueueData, 8> m_stQueuedBuffer;  // 缓存的缓冲区

        // 协程
        CoEvent m_stReadEvent;  // 等待读的协程

        // 回调
        ReadCallbackType m_stReadCallback;
    };
}
}
