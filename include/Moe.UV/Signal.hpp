/**
 * @file
 * @author chu
 * @date 2017/12/27
 */
#pragma once
#include "IoHandle.hpp"
#include "Coroutine.hpp"

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
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(CallbackType callback) { m_stCallback = callback; }

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
         * @return 是否主动取消，true指示事件触发
         *
         * 该方法只能在协程上执行。
         */
        bool CoWait();

    protected:
        void OnClose()noexcept override;
        void OnSignal(int signum)noexcept;

    private:
        ::uv_signal_t m_stHandle;

        int m_iWatchedSignum = 0;

        // 协程
        CoCondVar m_stSignalCondVar;  // 等待信号的协程

        // 回调
        CallbackType m_stCallback;
    };
}
}
