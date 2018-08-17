/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 信号（基类）
     */
    class SignalBase :
        public AsyncHandle
    {
    private:
        static void OnUVSignal(::uv_signal_t* handle, int signum)noexcept;

    protected:
        SignalBase();

    public:
        /**
         * @brief 激活句柄
         * @param signum 信号ID
         */
        void Start(int signum);

        /**
         * @brief 终止句柄
         */
        bool Stop()noexcept;

    protected:  // 需要实现
        virtual void OnSignal(int signum) = 0;

    private:
        ::uv_signal_t m_stHandle;
    };

    /**
     * @brief 信号
     */
    class Signal :
        public SignalBase
    {
    public:
        using OnSignalCallbackType = std::function<void(int)>;

        static UniqueAsyncHandlePtr<Signal> Create();
        static UniqueAsyncHandlePtr<Signal> Create(const OnSignalCallbackType& callback);
        static UniqueAsyncHandlePtr<Signal> Create(OnSignalCallbackType&& callback);

    protected:
        Signal() = default;

    public:
        /**
         * @brief 获取事件回调
         */
        const OnSignalCallbackType& GetOnSignalCallback()const noexcept { return m_pOnSignal; }

        /**
         * @brief 设置事件回调
         * @param type 事件类型
         */
        void SetOnSignalCallback(const OnSignalCallbackType& cb) { m_pOnSignal = cb; }

    protected:
        void OnSignal(int signum)override;

    private:
        OnSignalCallbackType m_pOnSignal;
    };
}
}
