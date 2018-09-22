/**
 * @file
 * @author chu
 * @date 2018/8/19
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

struct uv_async_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 异步唤醒对象
     */
    class AsyncNotifier :
        public AsyncHandle
    {
    public:
        using OnAsyncCallbackType = std::function<void()>;

        static AsyncNotifier Create();
        static AsyncNotifier Create(const OnAsyncCallbackType& callback);
        static AsyncNotifier Create(OnAsyncCallbackType&& callback);

    private:
        static void OnUVAsync(::uv_async_s* handle)noexcept;

    protected:
        using AsyncHandle::AsyncHandle;

    public:
        AsyncNotifier(AsyncNotifier&& org)noexcept;
        AsyncNotifier& operator=(AsyncNotifier&& rhs)noexcept;

    public:
        /**
         * @brief 激活对象
         *
         * 方法为线程安全。
         */
        void Notify();

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
        void SetOnAsyncCallback(OnAsyncCallbackType&& cb) { m_pOnAsync = std::move(cb); }

    protected:  // 事件
        void OnAsync();

    private:
        OnAsyncCallbackType m_pOnAsync;
    };
}
}
