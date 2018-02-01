/**
 * @file
 * @author chu
 * @date 2017/12/7
 */
#pragma once
#include "IoHandle.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 空闲句柄
     */
    class IdleHandle :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void()>;

        /**
         * @brief 创建空闲句柄
         * @param callback 句柄触发回调函数
         */
        static IoHandleHolder<IdleHandle> Create();
        static IoHandleHolder<IdleHandle> Create(const CallbackType& callback);
        static IoHandleHolder<IdleHandle> Create(CallbackType&& callback);

    private:
        static void OnUVIdle(uv_idle_t* handle)noexcept;

    protected:
        IdleHandle();

    public:
        /**
         * @brief 获取或设置回调函数
         */
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(const CallbackType& callback) { m_stCallback = callback; }
        void SetCallback(CallbackType&& callback)noexcept { m_stCallback = std::move(callback); }

        /**
         * @brief 启动Idle句柄
         */
        void Start();

        /**
         * @brief 停止Idle句柄
         */
        bool Stop()noexcept;

    protected:
        void OnIdle()noexcept;

    private:
        ::uv_idle_t m_stHandle;
        CallbackType m_stCallback;
    };
}
}
