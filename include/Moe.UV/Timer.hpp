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
    class Timer :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        static IoHandleHolder<Timer> Create(Time::Tick interval=100);

    private:
        static void OnUVTimer(::uv_timer_t* handle)noexcept;

    protected:
        Timer();
        ~Timer();

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
         * @brief 启动定时器
         */
        void Start();

        /**
         * @brief 终止定时器
         */
        bool Stop()noexcept;

        /**
         * @brief （协程）等待计时触发
         * @return 是否主动取消
         *
         * 该方法只能在协程上执行。
         */
        bool CoWait();

    protected:
        void OnClose()noexcept override;
        void OnTick()noexcept;

    private:
        ::uv_timer_t m_stHandle;

        // 属性
        Time::Tick m_ullInterval = 100;

        // 协程
        CoCondVar m_stTickCondVar;
    };
}
}
