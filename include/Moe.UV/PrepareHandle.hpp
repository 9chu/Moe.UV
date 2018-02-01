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
     * @brief 准备句柄
     */
    class PrepareHandle :
        public IoHandle
    {
        friend class ObjectPool;

    public:
        using CallbackType = std::function<void()>;

        /**
         * @brief 创建准备句柄
         * @param callback 句柄触发回调函数
         */
        static IoHandleHolder<PrepareHandle> Create();
        static IoHandleHolder<PrepareHandle> Create(const CallbackType& callback);
        static IoHandleHolder<PrepareHandle> Create(CallbackType&& callback);

    private:
        static void OnUVPrepare(uv_prepare_t* handle)noexcept;

    protected:
        PrepareHandle();

    public:
        /**
         * @brief 获取或设置回调函数
         */
        CallbackType GetCallback()const noexcept { return m_stCallback; }
        void SetCallback(const CallbackType& callback) { m_stCallback = callback; }
        void SetCallback(CallbackType&& callback)noexcept { m_stCallback = std::move(callback); }

        /**
         * @brief 启动Check句柄
         */
        void Start();

        /**
         * @brief 停止Check句柄
         */
        bool Stop()noexcept;

    protected:
        void OnPrepare()noexcept;

    private:
        ::uv_prepare_t m_stHandle;
        CallbackType m_stCallback;
    };
}
}
