/**
 * @file
 * @author chu
 * @date 2017/12/11
 */
#pragma once
#include "IoHandle.hpp"
#include "EndPoint.hpp"

namespace moe
{
namespace UV
{
    /**
     * @brief TCP套接字
     */
    class TcpSocket :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        static IoHandleHolder<TcpSocket> Create();

    private:
        struct QueueData
        {
            ObjectPool::BufferPtr Buffer;
            size_t Length;
        };

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
         * @brief 绑定套接字
         * @param address 绑定地址
         * @param reuse 是否复用
         * @param ipv6Only 是否仅IPV6地址
         */
        void Bind(const EndPoint& address, bool reuse=true, bool ipv6Only=false);

        /**
         * @brief （协程）连接地址
         * @param address 地址
         */
        void CoConnect(const EndPoint& address);

        /**
         * @brief 判断是否可读
         */
        bool IsReadable()const noexcept;

        /**
         * @brief 判断是否可写
         */
        bool IsWritable()const noexcept;

        /**
         * @brief （协程）关闭写端并等待数据全部写出
         */
        void CoShutdown();

        /**
         * @brief （无阻塞）发送数据包
         * @param buffer 数据（将被拷贝到内部缓冲区，调用后可以立即释放）
         */
        void Send(BytesView buffer);

        /**
         * @brief （协程）发送数据包并等待
         * @param address 地址
         * @param buffer 数据（无拷贝）
         *
         * 注意：
         * - 协程版本buffer中的数据不会被拷贝
         * - buffer不能分配在协程栈上，必须分配在堆内存上
         */
        void CoSend(BytesView buffer);

        /**
         * @brief （协程）接收数据包
         * @param[out] buffer 数据缓冲区
         * @param[out] size 数据大小
         * @return 是否收到数据，若为false表示操作被取消
         *
         * 当读取到EOF也会产生false表明操作被取消。
         */
        bool CoRead(ObjectPool::BufferPtr& buffer, size_t& size);

        /**
         * @brief 立即终止读操作
         */
        bool CancelRead()noexcept;

    protected:
        void OnClose()noexcept override;
        void OnError(int status)noexcept;
        void OnSend(size_t len)noexcept;
        void OnRead(const EndPoint& remote, ObjectPool::BufferPtr buffer, size_t len)noexcept;

    private:
        ::uv_tcp_t m_stHandle;
    };
}
}
