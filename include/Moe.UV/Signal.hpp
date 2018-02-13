/**
 * @file
 * @author chu
 * @date 2017/12/27
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
     * @brief 信号
     */
    class Signal :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void(int)>;

        /**
         * @brief 创建一个信号侦听器
         */
        static IoHandleHolder<Signal> Create();

    private:
        static void OnUVSignal(uv_signal_t* handle, int signum)noexcept;

    protected:
        Signal();

    public:
        /**
         * @brief 获取或设置信号触发回调函数
         */
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(const CallbackType& callback) { m_stCallback = callback; }
        void SetCallback(CallbackType&& callback)noexcept { m_stCallback = std::move(callback); }

        /**
         * @brief 启动信号
         */
        void Start(int signum);

        /**
         * @brief 终止信号回调
         */
        bool Stop()noexcept;

        /**
         * @brief （协程）等待信号触发
         * @param timeout 超时时间
         * @return 事件是否触发，若超时或者取消均会返回false
         *
         * 该方法只能在协程上执行。
         * 只允许单个协程进行等待。
         */
        bool CoWait(Time::Tick timeout=Coroutine::kInfinityTimeout);

        /**
         * @brief 取消协程等待
         */
        void CancelWait()noexcept;

    protected:
        void OnClose()noexcept override;
        void OnSignal(int signum)noexcept;

    private:
        ::uv_signal_t m_stHandle;

        int m_iWatchedSignum = 0;

        // 协程
        CoEvent m_stSignalEvent;  // 信号事件

        // 回调
        CallbackType m_stCallback;
    };
}
}
