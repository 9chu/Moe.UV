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
        class UVTimer;
    }

    /**
     * @brief 时钟
     */
    class Timer :
        public NonCopyable
    {
        friend class details::UVTimer;

    public:
        /**
         * @brief 构造时钟
         * @param interval 时钟间隔
         */
        Timer(Time::Tick interval=100)noexcept
            : m_ullInterval(interval) {}

        ~Timer();

        Timer(Timer&& rhs)noexcept;
        Timer& operator=(Timer&& rhs)noexcept;

    public:
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
         * @brief 启动计时器
         * @param background 是否为背景时钟，若设置为真，则即使时钟为活动事件主循环也不会等待。
         */
        void Start(bool background=false);

        /**
         * @brief 停止计时器
         */
        void Stop();

        /**
         * @brief 协程等待计时触发
         *
         * 该方法只能在协程上执行。
         */
        void CoWait();

    protected:  // 内部事件
        void OnTick()noexcept;

    private:
        // 句柄
        IOHandlePtr m_pHandle;

        // 属性
        Time::Tick m_ullInterval = 100;

        // 协程事件
        WaitHandle m_stCoTickWaitHandle;
    };
}
}
