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
     * @brief 事件类型
     */
    enum class EventType
    {
        Prepare,
        Idle,
        Check,
    };

    /**
     * @brief 事件回调句柄（基类）
     *
     * 处理循环事件的回调句柄。
     */
    class EventHandleBase :
        public AsyncHandle
    {
    private:
        union Events
        {
            ::uv_prepare_t Prepare;
            ::uv_idle_t Idle;
            ::uv_check_t Check;
        };

        static void OnUVPrepare(::uv_prepare_t* handle)noexcept;
        static void OnUVIdle(::uv_idle_t* handle)noexcept;
        static void OnUVCheck(::uv_check_t* handle)noexcept;

    protected:
        EventHandleBase(EventType type);

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

    protected:  // 需要实现
        virtual void OnEvent() = 0;

    private:
        Events m_stHandle;
        EventType m_uEventType;
    };

    /**
     * @brief 事件回调句柄
     *
     * 处理循环事件的回调句柄。
     */
    class EventHandle :
        public EventHandleBase
    {
    public:
        using OnEventCallbackType = std::function<void()>;

        static UniqueAsyncHandlePtr<EventHandle> Create(EventType type);
        static UniqueAsyncHandlePtr<EventHandle> Create(EventType type, const OnEventCallbackType& callback);
        static UniqueAsyncHandlePtr<EventHandle> Create(EventType type, OnEventCallbackType&& callback);

    protected:
        using EventHandleBase::EventHandleBase;

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

    protected:
        void OnEvent()override;

    private:
        OnEventCallbackType m_pOnEvent;
    };
}
}
