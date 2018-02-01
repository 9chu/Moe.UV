/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#pragma once
#include "IoHandle.hpp"
#include "CoEvent.hpp"

namespace moe
{
namespace UV
{
    /**
     * @brief 超时定时器
     * 
     * 提供单次触发回调的功能。
     */
    class Timeout :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void()>;

        /**
         * @brief 创建超时定时器
         */
        static IoHandleHolder<Timeout> Create();

    private:
        static void OnUVTimer(::uv_timer_t* handle)noexcept;

    protected:
        Timeout();

    public:
        /**
         * @brief 获取或设置定时器回调
         */
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(const CallbackType& callback) { m_stCallback = callback; }
        void SetCallback(CallbackType&& callback)noexcept { m_stCallback = std::move(callback); }

        /**
         * @brief 启动定时器
         * @param timeout 超时时间
         * @return 如果句柄被关闭则返回false
         */
        bool Start(Time::Tick timeout)noexcept;

        /**
         * @brief 终止定时器
         */
        void Stop()noexcept;

        /**
         * @brief （协程）等待计时触发
         * @return 是否主动取消
         *
         * 该方法只能在协程上执行。
         * 只允许单个协程进行等待。
         */
        bool CoWait();

        /**
         * @brief 取消等待的协程
         */
        void CancelWait()noexcept;

    protected:
        void OnClose()noexcept override;
        void OnTick()noexcept;

    private:
        ::uv_timer_t m_stHandle;

        // 协程
        CoEvent m_stTimeoutEvent;

        // 回调
        CallbackType m_stCallback;
    };
}
}
