/**
 * @file
 * @author chu
 * @date 2017/12/8
 */
#pragma once
#include "AsyncHandle.hpp"
#include "EndPoint.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief UDP套接字（基类）
     */
    class UdpSocketBase :
        public AsyncHandle
    {
        friend class ObjectPool;

    public:
        using OnSendCallbackType = std::function<void(uv_errno_t)>;

    private:
        static void OnUVSend(::uv_udp_send_t* request, int status)noexcept;
        static void OnUVAllocBuffer(::uv_handle_t* handle, size_t suggestedSize, ::uv_buf_t* buf)noexcept;
        static void OnUVRecv(::uv_udp_t* udp, ssize_t nread, const ::uv_buf_t* buf, const ::sockaddr* addr,
            unsigned flags)noexcept;

    protected:
        UdpSocketBase();

    public:
        /**
         * @brief 获取本地地址
         *
         * 如果失败，返回空地址。
         */
        EndPoint GetLocalEndPoint()const noexcept;

        /**
         * @brief 获取发送队列大小
         */
        size_t GetSendQueueSize()const noexcept;

        /**
         * @brief 获取发送队列数量
         */
        size_t GetSendQueueCount()const noexcept;

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
         *
         * 开始接受数据包。
         */
        void StartRead();

        /**
         * @brief 停止读取数据包
         *
         * 停止接受数据包。
         * 如果使用 Close 方法而不是 CancelRead 方法终止读操作，将会在回调函数中引发错误。
         */
        bool StopRead()noexcept;

        /**
         * @brief 发送数据包
         * @param address 地址
         * @param buf 数据
         *
         * 如果尚未绑定端口，则会绑定到0.0.0.0和随机端口上。
         * 方法将会拷贝数据，因此无需保持对数据的所有权。
         */
        void Send(const EndPoint& address, BytesView buf);

        /**
         * @brief 无拷贝地发送数据包
         * @param address 地址
         * @param buffer 数据
         * @param cb 回调函数
         *
         * 如果尚未绑定端口，则会绑定到0.0.0.0和随机端口上。
         */
        void SendNoCopy(const EndPoint& address, BytesView buffer, const OnSendCallbackType& cb);
        void SendNoCopy(const EndPoint& address, BytesView buffer, OnSendCallbackType&& cb);

        /**
         * @brief 尝试发送数据包
         * @param address 地址
         * @param buffer 数据
         * @return 是否成功发送
         */
        bool TrySend(const EndPoint& address, BytesView buffer);

    protected:  // 需要实现
        virtual void OnError(::uv_errno_t error) = 0;
        virtual void OnData(const EndPoint& remote, BytesView data) = 0;

    private:
        ::uv_udp_t m_stHandle;
    };

    /**
     * @brief UDP套接字
     */
    class UdpSocket :
        public UdpSocketBase
    {
    public:
        using OnSendCallbackType = UdpSocketBase::OnSendCallbackType;
        using OnErrorCallbackType = std::function<void(::uv_errno_t)>;
        using OnDataCallbackType = std::function<void(const EndPoint&, BytesView)>;

    public:
        static UniqueAsyncHandlePtr<UdpSocket> Create();

        /**
         * @brief 构造UDP套接字
         * @param bind 绑定端口
         * @param reuse 是否复用
         * @param ipv6Only 是否仅绑定IPV6地址
         */
        static UniqueAsyncHandlePtr<UdpSocket> Create(const EndPoint& bind, bool reuse=true, bool ipv6Only=false);

    public:
        const OnErrorCallbackType& GetOnErrorCallback()const noexcept { return m_pOnError; }
        void SetOnErrorCallback(const OnErrorCallbackType& cb) { m_pOnError = cb; }
        void SetOnErrorCallback(OnErrorCallbackType&& cb) { m_pOnError = std::move(cb); }

        const OnDataCallbackType& GetOnDataCallback()const noexcept { return m_pOnData; }
        void SetOnDataCallback(const OnDataCallbackType& cb) { m_pOnData = cb; }
        void SetOnDataCallback(OnDataCallbackType&& cb) { m_pOnData = std::move(cb); }

    protected:
        void OnError(::uv_errno_t error)override;
        void OnData(const EndPoint& remote, BytesView data)override;

    private:
        OnErrorCallbackType m_pOnError;
        OnDataCallbackType m_pOnData;
    };
}
}
