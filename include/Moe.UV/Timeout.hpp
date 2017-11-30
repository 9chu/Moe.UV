/**
 * @file
 * @author chu
 * @date 2017/11/30
 */
#pragma once
#include "IOHandle.hpp"
#include "Coroutine.hpp"

namespace moe
{
namespace UV
{
    namespace details
    {
        class UVTimeout;
    }

    /**
     * @brief 超时计时器
     */
    class Timeout :
        public NonCopyable
    {
        friend class details::UVTimeout;

    public:
        Timeout() = default;
        ~Timeout();

        Timeout(Timeout&& rhs)noexcept;
        Timeout& operator=(Timeout&& rhs)noexcept;

    public:
        /**
         * @brief 协程等待计时触发
         * @param interval 等待的计时时间
         * @return 如果超时则返回True，若取消则返回False
         *
         * 该方法只能在协程上执行。
         * 如果一个协程正在等待，另一个协程重入方法，则已协程将会重新等待timeout时间。
         */
        bool CoTimeout(Time::Tick timeout);

        /**
         * @brief 取消计时
         */
        void Cancel();

    protected:  // 内部事件
        void OnTick()noexcept;

    private:
        // 句柄
        IOHandlePtr m_pHandle;

        // 协程事件
        bool m_bCoTimeout = false;
        WaitHandle m_stCoTickWaitHandle;
    };
}
}
