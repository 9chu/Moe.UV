/**
 * @file
 * @author chu
 * @date 2018/2/1
 */
#pragma once
#include <stdexcept>
#include <Moe.Coroutine/ConditionVariable.hpp>

namespace moe
{
namespace UV
{
    /**
     * @brief 协程事件
     * @tparam T 事件类型
     *
     * 协程事件，用于在事件触发时激活协程并传递数据。
     * 出于简化实现考虑，目前一个事件仅允许单个协程等待。
     */
    class CoEvent
    {
    public:
        enum class WaitResult
        {
            Succeed,
            Cancelled,
            Timeout,
        };

    public:
        CoEvent();
        ~CoEvent();

    public:
        /**
         * @brief 等待事件
         * @param timeout 超时时间
         *
         * 在当前协程上等待。
         */
        WaitResult Wait(Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief 唤醒
         */
        void Resume();

        /**
         * @brief 取消等待
         */
        void Cancel();

        /**
         * @brief 抛出异常
         */
        void Throw(const std::exception_ptr& exception);

    private:
        bool m_bInWait = false;
        bool m_bCancelled = false;
        std::exception_ptr m_stException;

        RefPtr<Coroutine::ConditionVariable> m_pCondVar;
    };
}
}
