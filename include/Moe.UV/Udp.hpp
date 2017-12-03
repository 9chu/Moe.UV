/**
 * @file
 * @author chu
 * @date 2017/12/2
 */
#pragma once
#include "IOHandle.hpp"
#include "EndPoint.hpp"
#include "Coroutine.hpp"

namespace moe
{
namespace UV
{
    namespace details
    {
        class UVUdpSocket;
    };

    /**
     * @brief UDP套接字
     */
    class UdpSocket:
        public NonCopyable
    {
        friend class details::UVUdpSocket;

    public:
        UdpSocket();
        ~UdpSocket();

        UdpSocket(UdpSocket&& rhs)noexcept;
        UdpSocket& operator=(UdpSocket&& rhs)noexcept;

    public:
        /**
         * @brief 绑定UDP到端口
         * @param endpoint 对端
         *
         * - 如果尚未创建句柄，则立即创建UDP对象
         */
        void Bind(const EndPoint& endpoint);

        /**
         * @brief 取消所有等待的读操作并且停止读操作
         */
        void CancelRead();

        /**
         * @brief 发送数据（非阻塞）
         *
         * - 如果尚未创建句柄，则立即创建UDP对象
         * - 如果UDP尚未绑定则随机赋予一个本地地址
         */
        void Send();

        /**
         * @brief （协程）发起读操作
         *
         * - 如果尚未创建句柄，则立即创建UDP对象
         * - 如果UDP尚未绑定则随机赋予一个本地地址
         * - 如果UDP尚未进行读操作则发起读操作
         * - 如果没有及时调用Read，UDP对象会缓存少数未被处理的数据，当缓冲区积累到一定大小则会抛弃之后的所有数据
         */
        bool CoRead();

        /**
         * @brief （协程）发起写操作并且等待数据发送完毕
         *
         * - 如果尚未创建句柄，则立即创建UDP对象
         * - 如果UDP尚未绑定则随机赋予一个本地地址
         */
        void CoSend();

    private:
        // 句柄
        IOHandlePtr m_pHandle;

        // 协程事件
        WaitHandle m_stCoReadWaitHandle;
    };
}
}
