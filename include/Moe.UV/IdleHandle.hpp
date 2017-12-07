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

        static IoHandleHolder<IdleHandle> Create();
        static IoHandleHolder<IdleHandle> Create(CallbackType callback);

    private:
        static void OnUVIdle(uv_idle_t* handle)noexcept;

    protected:
        IdleHandle();

    public:
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(CallbackType callback) { m_stCallback = callback; }

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
