/**
 * @file
 * @author chu
 * @date 2018/8/17
 */
#pragma once
#include <Moe.Core/Time.hpp>
#include "AsyncHandle.hpp"

#include <functional>

struct uv_timer_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 计时器
     */
    class Timer :
        public AsyncHandle
    {
    public:
        using OnTimeCallbackType = std::function<void()>;

        static Timer Create();
        static Timer CreateTickTimer(Time::Tick interval);

    private:
        static void OnUVTimer(::uv_timer_s* handle)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        Timer(Timer&& org)noexcept;
        Timer& operator=(Timer&& rhs)noexcept;

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

    public:
        OnTimeCallbackType GetOnTimeCallback()const noexcept { return m_stOnTime; }
        void SetOnTimeCallback(const OnTimeCallbackType& cb) { m_stOnTime = cb; }
        void SetOnTimeCallback(OnTimeCallbackType&& cb) { m_stOnTime = std::move(cb); }

    protected:  // 事件
        void OnTime();

    private:
        Time::Tick m_ullFirstTime = 0;
        Time::Tick m_ullInterval = 0;

        OnTimeCallbackType m_stOnTime;
    };
}
}
