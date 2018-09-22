/**
 * @file
 * @author chu
 * @date 2018/9/2
 */
#pragma once
#include "Stream.hpp"

#include <functional>

struct uv_connect_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 管道
     */
    class Pipe :
        public Stream
    {
    public:
        using OnConnectCallbackType = std::function<void()>;

        static Pipe Create(bool ipc=false);

    private:
        static void OnUVConnect(::uv_connect_s* request, int status)noexcept;

    protected:
        using Stream::Stream;

    public:
        Pipe(Pipe&& org)noexcept;
        Pipe& operator=(Pipe&& rhs)noexcept;

    public:
        /**
         * @brief 打开一个现成的句柄
         * @param fd 系统文件句柄
         */
        void Open(int fd);

        /**
         * @brief 绑定到一个文件路径（UNIX）或者命名管道上（WINDOWS）
         * @param name 路径或者名字
         */
        void Bind(const char* name);

        /**
         * @brief 连接到UNIX DOMAIN SOCKET或者命名管道上（WINDOWS）
         * @param name 路径或者名字
         */
        void Connect(const char* name);

        /**
         * @brief 获取本地名称
         */
        std::string GetSockName();

        /**
         * @brief 获取对端名称
         */
        std::string GetPeerName();

    public:
        const OnConnectCallbackType& GetOnConnectCallback()const noexcept { return m_pOnConnect; }
        void SetOnConnectCallback(const OnConnectCallbackType& cb) { m_pOnConnect = cb; }
        void SetOnConnectCallback(OnConnectCallbackType&& cb)noexcept { m_pOnConnect = std::move(cb); }

    protected:  // 事件
        void OnConnect();

    private:
        OnConnectCallbackType m_pOnConnect;
    };
}
}
