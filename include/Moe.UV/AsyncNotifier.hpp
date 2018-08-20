/**
 * @file
 * @author chu
 * @date 2018/8/19
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

namespace moe
{
namespace UV
{
    /**
     * @brief 异步唤醒对象（基类）
     */
    class AsyncNotifierBase :
        public AsyncHandle
    {
    private:
        static void OnUVAsync(::uv_async_t* handle)noexcept;

    protected:
        AsyncNotifierBase();

    public:
        /**
         * @brief 激活对象
         *
         * 方法为线程安全。
         */
        void Notify();

    protected:  // 需要实现
        virtual void OnAsync() = 0;

    private:
        ::uv_async_t m_stHandle;
    };

    /**
     * @brief 异步唤醒对象
     */
    class AsyncNotifier :
        public AsyncNotifierBase
    {
    public:
        using OnAsyncCallbackType = std::function<void()>;

        static UniqueAsyncHandlePtr<AsyncNotifier> Create();
        static UniqueAsyncHandlePtr<AsyncNotifier> Create(const OnAsyncCallbackType& callback);
        static UniqueAsyncHandlePtr<AsyncNotifier> Create(OnAsyncCallbackType&& callback);

    protected:
        AsyncNotifier() = default;

    public:
        /**
         * @brief 获取事件回调
         */
        const OnAsyncCallbackType& GetOnAsyncCallback()const noexcept { return m_pOnAsync; }

        /**
         * @brief 设置事件回调
         * @param type 事件类型
         */
        void SetOnAsyncCallback(const OnAsyncCallbackType& cb) { m_pOnAsync = cb; }

    protected:
        void OnAsync()override;

    private:
        OnAsyncCallbackType m_pOnAsync;
    };
}
}
