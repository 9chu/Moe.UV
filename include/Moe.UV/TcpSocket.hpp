/**
 * @file
 * @author chu
 * @date 2018/9/20
 */
#pragma once
#include "Stream.hpp"
#include "EndPoint.hpp"

#include <functional>

struct uv_connect_s;

namespace moe
{
namespace UV
{
    /**
     * @brief TCP套接字
     */
    class TcpSocket :
        public Stream
    {
    public:
        using OnConnectCallbackType = std::function<void()>;
        using OnConnectionCallbackType = std::function<void()>;

        static TcpSocket Create();

    private:
        static void OnUVConnect(::uv_connect_s* request, int status)noexcept;
        static void OnUVConnection(::uv_stream_s* handle, int status)noexcept;

    protected:
        using Stream::Stream;

    public:
        TcpSocket(TcpSocket&& org)noexcept;
        TcpSocket& operator=(TcpSocket&& rhs)noexcept;

    public:
        /**
         * @brief 打开一个现成的句柄
         * @param fd 系统套接字句柄
         */
        void Open(int fd);

        /**
         * @brief 设置是否启用Nagle算法
         * @param enable 开关
         */
        void SetNoDelay(bool enable);

        /**
         * @brief 设置TCP Keep-alive
         * @param enable 是否启用
         * @param delay 首次时延
         */
        void SetKeepAlive(bool enable, unsigned delay);

        /**
         * @brief 设置SimultaneousAccepts
         * @param enable 是否启用
         */
        void SetSimultaneousAccepts(bool enable);

        /**
         * @brief 绑定到地址
         * @param addr 地址
         * @param ipv6Only 是否仅IPV6生效
         */
        void Bind(const EndPoint& addr, bool ipv6Only);

        /**
         * @brief 连接到地址
         * @param addr 地址
         */
        void Connect(const EndPoint& addr);

        /**
         * @brief 开始侦听
         * @param backlog 后备队列大小
         */
        void Listen(unsigned backlog=128);

        /**
         * @brief 接受连接
         * @return TCP对象
         */
        TcpSocket Accept();

        /**
         * @brief 获取本地名称
         */
        EndPoint GetSockName();

        /**
         * @brief 获取对端名称
         */
        EndPoint GetPeerName();

    public:
        const OnConnectCallbackType& GetOnConnectCallback()const noexcept { return m_pOnConnect; }
        void SetOnConnectCallback(const OnConnectCallbackType& cb) { m_pOnConnect = cb; }
        void SetOnConnectCallback(OnConnectCallbackType&& cb)noexcept { m_pOnConnect = std::move(cb); }

        const OnConnectionCallbackType& GetOnConnectionCallback()const noexcept { return m_pOnConnection; }
        void SetOnConnectionCallback(const OnConnectionCallbackType& cb) { m_pOnConnection = cb; }
        void SetOnConnectionCallback(OnConnectionCallbackType&& cb)noexcept { m_pOnConnection = std::move(cb); }

    protected:  // 事件
        void OnConnect();
        void OnConnection();

    private:
        OnConnectCallbackType m_pOnConnect;
        OnConnectionCallbackType m_pOnConnection;
    };
}
}
