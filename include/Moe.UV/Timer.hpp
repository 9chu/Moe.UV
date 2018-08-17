/**
 * @file
 * @author chu
 * @date 2018/8/17
 */
#pragma once
#include "AsyncHandle.hpp"

#include <Moe.Core/Time.hpp>
#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 计时器（基类）
     */
    class TimerBase :
        public AsyncHandle
    {
    private:
        static void OnUVTimer(::uv_timer_t* handle)noexcept;

    protected:
        TimerBase();

    public:
        /**
         * @brief 获取首次激活时间
         */
        Time::Tick GetFirstTime()const noexcept { return m_ullFirstTime; }

        /**
         * @brief 获取时间间隔
         */
        Time::Tick GetInterval()const noexcept { return m_ullInterval; }

        /**
         * @brief 设置首次激活时间
         * @param t 时间
         */
        void SetFirstTime(Time::Tick t)noexcept { m_ullFirstTime = t; }

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

    protected:  // 需要实现
        virtual void OnTime() = 0;

    private:
        ::uv_timer_t m_stHandle;

        // 属性
        Time::Tick m_ullFirstTime = 0;
        Time::Tick m_ullInterval = 0;
    };

    /**
     * @brief 计时器
     */
    class Timer :
        public TimerBase
    {
    public:
        using OnTimeCallbackType = std::function<void()>;

        static UniqueAsyncHandlePtr<Timer> Create();
        static UniqueAsyncHandlePtr<Timer> CreateTickTimer(Time::Tick interval);

    protected:
        Timer() = default;

    public:
        OnTimeCallbackType GetOnTimeCallback()const noexcept { return m_stOnTime; }
        void SetOnTimeCallback(const OnTimeCallbackType& cb) { m_stOnTime = cb; }
        void SetOnTimeCallback(OnTimeCallbackType&& cb) { m_stOnTime = std::move(cb); }

    protected:
        void OnTime()override;

    private:
        OnTimeCallbackType m_stOnTime;
    };
}
}
