/**
 * @file
 * @author chu
 * @date 2017/12/10
 */
#pragma once
#include "AsyncHandle.hpp"

#include <functional>

struct uv_prepare_s;
struct uv_idle_s;
struct uv_check_s;

namespace moe
{
namespace UV
{
    /**
     * @brief 事件回调句柄
     *
     * 处理循环事件的回调句柄。
     */
    class EventHandle :
        public AsyncHandle
    {
    public:
        /**
         * @brief 事件类型
         */
        enum class EventType
        {
            Prepare,
            Idle,
            Check,
        };

        using OnEventCallbackType = std::function<void()>;

        static EventHandle Create(EventType type);
        static EventHandle Create(EventType type, const OnEventCallbackType& callback);
        static EventHandle Create(EventType type, OnEventCallbackType&& callback);

    private:
        static void OnUVPrepare(::uv_prepare_s* handle)noexcept;
        static void OnUVIdle(::uv_idle_s* handle)noexcept;
        static void OnUVCheck(::uv_check_s* handle)noexcept;

    protected:
        EventHandle(EventType type, UniquePooledObject<::uv_handle_s>&& handle);

    public:
        EventHandle(EventHandle&& org)noexcept;
        EventHandle& operator=(EventHandle&& rhs)noexcept;

    public:
        /**
         * @brief 获得对应的事件类型
         */
        EventType GetType()const noexcept { return m_uEventType; }

        /**
         * @brief 激活句柄
         */
        void Start();

        /**
         * @brief 终止句柄
         */
        bool Stop()noexcept;

    public:
        /**
         * @brief 获取事件回调
         */
        const OnEventCallbackType& GetOnEventCallback()const noexcept { return m_pOnEvent; }

        /**
         * @brief 设置事件回调
         * @param type 事件类型
         */
        void SetOnEventCallback(const OnEventCallbackType& cb) { m_pOnEvent = cb; }
        void SetOnEventCallback(OnEventCallbackType&& cb) { m_pOnEvent = std::move(cb); }

    protected:  // 事件
        void OnEvent();

    private:
        EventType m_uEventType;

        OnEventCallbackType m_pOnEvent;
    };
}
}
