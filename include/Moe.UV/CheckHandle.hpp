/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#pragma once
#include "IoHandle.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 检查句柄
     */
    class CheckHandle :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void()>;

        static IoHandleHolder<CheckHandle> Create();
        static IoHandleHolder<CheckHandle> Create(CallbackType callback);

    private:
        static void OnUVCheck(uv_check_t* handle)noexcept;

    protected:
        CheckHandle();

    public:
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(CallbackType callback) { m_stCallback = callback; }

        /**
         * @brief 启动Check句柄
         */
        void Start();

        /**
         * @brief 停止Check句柄
         */
        bool Stop()noexcept;

    protected:
        void OnCheck()noexcept;

    private:
        ::uv_check_t m_stHandle;
        CallbackType m_stCallback;
    };
}
}
