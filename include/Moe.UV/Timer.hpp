/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#pragma once
#include "IoHandle.hpp"
#include "CoEvent.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 定时器
     *
     * 提供一定时间间隔执行回调函数的功能。
     */
    class Timer :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void()>;

        /**
         * @brief 创建定时器
         * @param interval 时间间隔
         */
        static IoHandleHolder<Timer> Create(Time::Tick interval=100);

    private:
        static void OnUVTimer(::uv_timer_t* handle)noexcept;

    protected:
        Timer();

    public:
        /**
         * @brief 获取或设置定时器回调
         */
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(const CallbackType& callback) { m_stCallback = callback; }
        void SetCallback(CallbackType&& callback)noexcept { m_stCallback = std::move(callback); }

        /**
         * @brief 获取时间间隔
         */
        Time::Tick GetInterval()const noexcept { return m_ullInterval; }

        /**
         * @brief 设置计时间隔
         *
         * 如果需要在计时器执行时调整间隔，需要先Stop再Start。
         */
        void SetInterval(Time::Tick interval)noexcept { m_ullInterval = interval; }

        /**
         * @brief 启动定时器
         * @return 如果句柄被关闭则返回false
         */
        bool Start()noexcept;

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

        // 属性
        Time::Tick m_ullInterval = 100;

        // 协程
        CoEvent m_stTickEvent;

        // 回调
        CallbackType m_stCallback;
    };
}
}
