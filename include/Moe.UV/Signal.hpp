/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

struct uv_signal_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 信号
     */
    class Signal :
        public AsyncHandle
    {
    public:
        using OnSignalCallbackType = std::function<void(int)>;

        static Signal Create();
        static Signal Create(const OnSignalCallbackType& callback);
        static Signal Create(OnSignalCallbackType&& callback);

    private:
        static void OnUVSignal(::uv_signal_s* handle, int signum)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        Signal(Signal&& org)noexcept;
        Signal& operator=(Signal&& rhs)noexcept;

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
        void SetOnSignalCallback(OnSignalCallbackType&& cb) { m_pOnSignal = std::move(cb); }

    protected:  // 事件
        void OnSignal(int signum);

    private:
        OnSignalCallbackType m_pOnSignal;
    };
}
}
