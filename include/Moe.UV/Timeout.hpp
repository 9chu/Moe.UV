/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#pragma once
#include "IoHandle.hpp"
#include "Coroutine.hpp"

namespace moe
{
namespace UV
{
    class Timeout :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        static IoHandleHolder<Timeout> Create();

    private:
        static void OnUVTimer(::uv_timer_t* handle)noexcept;

    protected:
        Timeout();
        ~Timeout();

    public:
        /**
         * @brief （协程）等待计时触发
         * @param timeout 等待的计时时间
         * @return 如果超时则返回True，若取消则返回False
         *
         * 该方法只能在协程上执行。
         * 如果一个协程正在等待，另一个协程重入方法，则已等待的协程将会重新等待timeout时间。
         */
        bool CoTimeout(Time::Tick timeout);

        /**
         * @brief 取消计时
         */
        bool Cancel()noexcept;

    protected:
        void OnClose()noexcept override;
        void OnTick()noexcept;

    private:
        ::uv_timer_t m_stHandle;

        // 协程
        CoCondVar m_stTickCondVar;
    };
}
}
